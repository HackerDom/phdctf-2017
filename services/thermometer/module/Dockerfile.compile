FROM alpine:3.4

RUN apk add --no-cache make gcc musl-dev libuuid libc6-compat openssl-dev util-linux-dev libmicrohttpd-dev mariadb-dev bash

COPY paho.mqtt.c /usr/src/paho.mqtt.c
WORKDIR /usr/src/paho.mqtt.c
RUN make

COPY src /usr/src/module
WORKDIR /usr/src/module
RUN make && make install
