CC = gcc
CFLAGS = -g -Wall 

all: myshell

myshell: shell2.o utils_shell.o
	$(CC) $(CFLAGS) -o myshell shell2.o utils_shell.o -lreadline

shell2.o: shell2.c utils_shell.o shell.h
	$(CC) $(CFLAGS) -c shell2.c -o shell2.o -lreadline

utils_shell.o: shell.h
	$(CC) $(CFLAGS) -c utils_shell.c -o utils_shell.o -lreadline

clean:
	rm -f *.o *.a *.so myshell


