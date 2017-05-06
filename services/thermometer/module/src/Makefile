all: module.c
	gcc -o module module.c -I/usr/include/ -I/usr/include/mysql -lmicrohttpd -lpaho-mqtt3c -lmysqlclient -luuid
install:
	cp module /usr/bin/
clean:
	rm -f module
