CC=gcc
CFLAGS=-I.


all : server client

server : multmodulo.c multmodulo.h server.c
	$(CC) -o server server.c multmodulo.c $(CFLAGS)

client : multmodulo.c multmodulo.h client.c
	$(CC) -o client client.c multmodulo.c $(CFLAGS)

clean :
	rm server client