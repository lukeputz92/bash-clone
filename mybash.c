#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h> 
#include <sys/wait.h>
#include <fcntl.h>
#include "parser.h"

extern char **environ;
char built_in[6][10] = {"cd\0", "pwd\0", "exit\0", "set\0","DEBUG=yes\n", "DEBUG=no\n" };

int built_in_command(struct CommandData *data);
void debug_print(struct CommandData *data);
void exec_built_in(struct CommandData *data);
void exec_command(struct CommandData *data);
void execPipe(struct CommandData *data);
void clearData(struct CommandData *data);

int main(int argc, char *argv[])
{
    char cwd[100];
    char input[100];
    char *input_buff, *cwd_buff;
    struct CommandData *data = malloc(sizeof(*data));
    int debug_mode = 0;

    while (1){
        cwd_buff = getcwd(cwd,sizeof(cwd));
        printf("%s/> ",cwd_buff );
        input_buff = fgets(input, 100, stdin);
        if (strncmp(input_buff,"DEBUG=yes\n",10) == 0){
            debug_mode = 1;
            printf("Debugging is on\n");
        } else if (strncmp(input,"DEBUG=no\n",9) == 0){
            debug_mode = 0;
            printf("Debugging is off\n");
        } else if (ParseCommandLine(input,data)){

            if (debug_mode)
                debug_print(data);
            if (data->numcommands > 1)
                execPipe(data);
            else if (built_in_command(data))
                exec_built_in(data);
            else
                exec_command(data);
            
        } else
            printf("Parsing Error\n");
        clearData(data);
    }

    exit(0);
}


int built_in_command(struct CommandData* data){
    if (data->numcommands == 1){
                if (strncmp(data->TheCommands[0].command, built_in[0], 3) == 0 ||
                    strncmp(data->TheCommands[0].command, built_in[1], 4) == 0 ||
                    strncmp(data->TheCommands[0].command, built_in[2], 5) == 0 ||
                    strncmp(data->TheCommands[0].command, built_in[3], 4) == 0 ||
                    strncmp(data->TheCommands[0].command, built_in[4], 10) == 0 ||
                    strncmp(data->TheCommands[0].command, built_in[5], 9) == 0 ){

                    return 1;
                }
    }
    return 0;
}


void debug_print(struct CommandData *data){
    printf("\nDEBUG OUTPUT:\n");
    printf("--------------------------\n");
    printf("Number of simple commands : %d\n", data->numcommands);
    for (int i = 0; i < data->numcommands; i++){
        printf("command%d    : %s\n",i+1, data->TheCommands[i].command);
        for (int j = 0; j < data->TheCommands[i].numargs; j++)
        printf("arg[%d]      : %s\n",j,  data->TheCommands[i].args[j]);
    }
    printf("Input file  : %s\n", data->infile);
    printf("Output file : %s\n", data->outfile);
    if (data->background)
        printf("Background option : ON\n");
    else
        printf("Background option : OFF\n");
    if (built_in_command(data)){
            printf("Built-in command  : YES\n");
    } else
        printf("Built-in command  : NO\n");
    printf("--------------------------\n");
    return;

}

void exec_built_in(struct CommandData *data){
    int ret, fd, in, out;
    in = dup(0);
    out = dup(1);
    if (data->infile != NULL) {
        fd = open(data->infile, O_RDONLY, 0);
        dup2(fd, STDIN_FILENO);
        close(fd);
    }
    if (data->outfile != NULL) {
        fd = open(data->outfile, O_CREAT | O_TRUNC | O_WRONLY, 0600);
        dup2(fd, STDOUT_FILENO);
        close(fd);
    }
    if (strncmp(data->TheCommands[0].command, "cd", 2) == 0 
            && data->TheCommands[0].numargs > 0) {
        ret = chdir(data->TheCommands[0].args[0]);
        if (ret == -1)
            perror("");
    }
    else if (strncmp(data->TheCommands[0].command, built_in[0], 3) == 0) {
        ret = chdir(getenv("HOME"));
        if (ret == -1)
            perror("");

    }
    else if (strncmp(data->TheCommands[0].command, built_in[1], 4) == 0) {
        printf("%s\n", getenv("PWD"));
    }
    else if (strncmp(data->TheCommands[0].command, built_in[2], 5) == 0)
        exit(0);
    else if (strncmp(data->TheCommands[0].command, built_in[3], 4) == 0) {
        int i; 
        for (i = 0; environ[i] != NULL; i++) 
            printf("%s\n", environ[i]);
        printf("\n");
    }
    if (data->infile != NULL) {
        dup2(in, STDIN_FILENO);
        close(in);
    }
    if (data->outfile != NULL) {
        dup2(out, STDOUT_FILENO);
        close(out);
    }
    
}

void exec_command(struct CommandData *data) {
    int fd;
    char *args[13];
    args[0] = data->TheCommands[0].command;
    args[12] = NULL;
    for (int i = 0; i < 11; ++i)
        args[i + 1] = data->TheCommands[0].args[i];
    // printf("%s\n", data->TheCommands[0].command);
    pid_t pid = fork();
    if (pid == -1 ) {
        printf("\nFailed forking child"); 
        return;
    }
    else if (pid == 0) {
        if (data->infile != NULL) {
                fd = open(data->infile, O_RDONLY, 0);
                dup2(fd, STDIN_FILENO);
                close(fd);
            }
            if (data->outfile != NULL) {
                fd = open(data->outfile, O_CREAT | O_TRUNC | O_WRONLY, 0600);
                dup2(fd, STDOUT_FILENO);
                close(fd);
            }
        if (execvp(data->TheCommands[0].command, args) < 0) {
            printf("\nCould not execute the command.");
            kill(getpid(),SIGTERM);
        }
    }
    else if (data->background == 0) {
        waitpid(pid, NULL, 0);
        return;
    }
}

void execPipe(struct CommandData *data) {
    int pipefd[2], fd;  
    pid_t pid1, pid2;
    char *args1[13];
    args1[0] = data->TheCommands[0].command;
    args1[12] = NULL;
    char *args2[13];
    args2[0] = data->TheCommands[1].command;
    args2[12] = NULL;
    for (int i = 0; i < 11; ++i) {
        args1[i + 1] = data->TheCommands[0].args[i];
        args2[i + 1] = data->TheCommands[1].args[i];
    }
    // printf("%s\n", data->TheCommands[0].command);
    if(pipe(pipefd) < 0) {
        printf("%s\n", "Failed to intialize pipe");
        return;
    }
    pid1 = fork();

    if (pid1 == -1 ) {
        printf("%s\n", "Failed forking child of pid1"); 
        return;
    }
    else if (pid1 == 0) {
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[0]);
        if (data->infile != NULL) {
                fd = open(data->infile, O_RDONLY, 0);
                dup2(fd, STDIN_FILENO);
                close(fd);
        }

        if (execvp(data->TheCommands[0].command, args1) < 0) {
            printf("\nCould not execute command 1.");
            kill(getpid(),SIGTERM);
        }
    }
    else {
        pid2 = fork();
        if (pid2 == -1) {
            printf("%s\n", "Failed forking child of pid2");
            return; 
        }
        else if (pid2 == 0) {
            dup2(pipefd[0], STDIN_FILENO);
            close(pipefd[1]);
            if (data->outfile != NULL) {
                fd = open(data->outfile, O_CREAT | O_TRUNC | O_WRONLY, 0600);
                dup2(fd, STDOUT_FILENO);
                close(fd);
            }
            if (execvp(data->TheCommands[1].command, args2) < 0) {
                printf("\nCould not execute command 2.");
                kill(getpid(),SIGTERM);
            }
        }
        else if (data->background == 0) {
            close(pipefd[0]);
            close(pipefd[1]);
            waitpid(pid1, NULL, 0);
            waitpid(pid2, NULL, 0);
            return;
        }
    }

}


void clearData(struct CommandData *data) {
    for (int i = 0; i < 2; ++i) {
        data->TheCommands[i].command = NULL;
        data->TheCommands[i].numargs = 0;
    }
    for (int i = 0; i < 2; ++i) {
        for(int j = 0; j < 11; ++j) {
            data->TheCommands[i].args[j] = NULL;
        }
    }
    data->numcommands = 0;
    data->infile = NULL;
    data->outfile = NULL;
    data->background = 0;
}

