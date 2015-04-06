
PROG=unfit
OBJECTS=unfit.o
CFLAGS=-g -Wall -Werror

all: ${PROG}

${PROG}: ${OBJECTS}
	gcc ${OBJECTS} -o ${PROG}

unfit.o: unfit.c

.c.o:
	gcc ${CFLAGS} -c $< -o $*.o

clean:
	rm ${PROG} ${OBJECTS}
