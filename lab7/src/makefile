CC=gcc
CFLAGS=-I.


all : tcpserver tcpclient udpserver udpclient

tcpserver : tcpserver.c
	$(CC) -o tcpserver tcpserver.c $(CFLAGS)

tcpclient : tcpclient.c
	$(CC) -o tcpclient tcpclient.c $(CFLAGS)

udpserver : udpserver.c
	$(CC) -o udpserver udpserver.c $(CFLAGS)

udpclient : udpclient.c
	$(CC) -o udpclient udpclient.c $(CFLAGS)

clean :
	rm tcpserver tcpclient udpserver udpclient