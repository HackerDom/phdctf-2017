coap-server: coap-server.cpp
	g++ -I./include -I./include/coap -isystem./include/coap -pedantic -Wall \
		-g -O2 -fdiagnostics-color -D_GNU_SOURCE -DWITH_POSIX \
		-MT coap-server.o -MD -MP -c -o coap-server.o coap-server.cpp
	gcc -isystem../include/coap -I../include/coap -pedantic -Wall \
		-std=c99 -g -O2 -fdiagnostics-color -D_GNU_SOURCE -DWITH_POSIX \
		-o coap-server coap-server.o  libs/libcoap-1.a

clean:
	rm coap-server coap-server.o coap-server.d
