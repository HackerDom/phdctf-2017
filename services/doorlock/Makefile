coap-server: coap-server.c
	gcc -I./include -I./include/coap -isystem./include/coap -pedantic -Wall -Wextra -Wformat-security -Winline -Wmissing-declarations -Wmissing-prototypes -Wnested-externs -Wpointer-arith -Wshadow -Wstrict-prototypes -Wswitch-default -Wswitch-enum -Wunused  -Wlogical-op -Wunused-result -g -O2 -fdiagnostics-color -D_GNU_SOURCE -DWITH_POSIX -MT coap-server.o -MD -MP -c -o coap-server.o coap-server.c
	gcc -isystem../include/coap -I../include/coap -pedantic -Wall -Wextra -Wformat-security -Winline -Wmissing-declarations -Wmissing-prototypes -Wnested-externs -Wpointer-arith -Wshadow -Wstrict-prototypes -Wswitch-default -Wswitch-enum -Wunused -Wlogical-op -Wunused-result -std=c99 -g -O2 -fdiagnostics-color -D_GNU_SOURCE -DWITH_POSIX -o coap-server coap-server.o  libs/libcoap-1.a

clean:
	rm coap-server coap-server.o coap-server.d
