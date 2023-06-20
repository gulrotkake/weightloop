CC=gcc
LFLAGS=-I. -Wall
CFLAGS=-I. -Wall
wii: weightloop.o
	$(CC) -o weightloop weightloop.o $(LFLAGS)

wii.o: weightloop.c
	$(CC) $(CFLAGS) -c weightloop.c -o weightloop.o
