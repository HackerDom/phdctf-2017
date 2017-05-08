doorlock-server: doorlock-server.cpp
	g++ -I./include -I./include/coap -isystem./include/coap -pedantic -Wall \
		-Wno-write-strings -Wno-sign-compare \
		-g -O2 -fdiagnostics-color -D_GNU_SOURCE -DWITH_POSIX \
		-MT doorlock-server.o -MD -MP -c -o doorlock-server.o doorlock-server.cpp
	gcc -isystem../include/coap -I../include/coap -pedantic -Wall \
		-std=c99 -g -O2 -fdiagnostics-color -D_GNU_SOURCE -DWITH_POSIX \
		-o doorlock-server doorlock-server.o  libs/libcoap-1.a -lldap /usr/lib/x86_64-linux-gnu/liblber.a

clean:
	rm doorlock-server doorlock-server.o doorlock-server.d
