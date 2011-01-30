# Generic variables
INCLUDE=include
SRC=src
BIN=bin

# Used programs (compiler, archiver, ...)
CC=/usr/bin/gcc

# Program flags
CPPFLAGS=-D BUFFER_SIZE=512 -D SERVER_LISTEN_PORT=9900
CCFLAGS=-Wall -ansi -g -pedantic -O
LDFLAGS=-lrt -lpthread

# Makefile targets
all: ${BIN}/server ${BIN}/client

${BIN}/server:
	${CC} ${CPPFLAGS} ${CCFLAGS} ${LDFLAGS} -o ${BIN}/server ${SRC}/server.c ${SRC}/list.c

${BIN}/client:
	${CC} ${CPPFLAGS} ${CCFLAGS} -o ${BIN}/client ${SRC}/client.c

clean:
	rm -f ${BIN}/*

test:
	valgrind --leak-check=full ${BIN}/server
