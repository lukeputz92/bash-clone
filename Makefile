CFLAGS = -g -Wall -O2
CC = gcc

PROGRAM = mybash
CFILES = parser.c mybash.c
HFILE = parser.h

# compute the OFILES
OFILES = ${CFILES:.c=.o}

# all of the .o files that the program needs
OBJECTS = ${OFILES}


# How to make the whole program
${PROGRAM} : ${OBJECTS}
	${CC} ${CFLAGS} ${OBJECTS} -o ${PROGRAM}

${OFILES}: ${HFILE} parser.h

clean:
	/bin/rm -f *.o ${PROGRAM} *~

