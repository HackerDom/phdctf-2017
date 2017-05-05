all: module.c
	gcc -o module module.c -I/usr/include/ -lmicrohttpd -lpaho-mqtt3c
install:
	cp module /usr/bin/
clean:
	rm -f module
