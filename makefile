CC = gcc
CFLAGS = -g -Wall 

all: myshell

myshell: shell.o utils_shell.o
	$(CC) $(CFLAGS) -o myshell shell.o utils_shell.o -lreadline

shell.o: shell.c utils_shell.o shell.h
	$(CC) $(CFLAGS) -c shell.c -o shell.o -lreadline

utils_shell.o: shell.h
	$(CC) $(CFLAGS) -c utils_shell.c -o utils_shell.o -lreadline

clean:
	rm -f *.o *.a *.so myshell


