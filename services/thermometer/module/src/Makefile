all: server.c
	gcc -o server server.c -I/usr/include/ -lmicrohttpd
install:
	cp server /usr/bin/
clean:
	rm -f server
