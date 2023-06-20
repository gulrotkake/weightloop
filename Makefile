CC=gcc
LFLAGS=-I. -Wall `pkg-config --libs glib-2.0 gio-2.0 libxwiimote`
CFLAGS=-I. -Wall `pkg-config --cflags glib-2.0 gio-2.0 libxwiimote`
wii: weightloop.o
	$(CC) -o weightloop weightloop.o $(LFLAGS)

wii.o: weightloop.c
	$(CC) $(CFLAGS) -c weightloop.c -o weightloop.o
