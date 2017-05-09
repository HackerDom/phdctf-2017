FROM alpine:3.4

RUN apk add --no-cache make gcc g++ musl-dev util-linux-dev mariadb-dev

COPY mosquitto /usr/mosquitto
COPY mosquitto-config.mk /usr/mosquitto/config.mk

COPY mosquitto-auth-plug /usr/mosquitto-auth-plug
COPY mosquitto-auth-plug-config.mk /usr/mosquitto-auth-plug/config.mk
COPY be-mysql.c /usr/mosquitto-auth-plug/

WORKDIR /usr/mosquitto
RUN make binary
WORKDIR /usr/mosquitto-auth-plug
RUN make

VOLUME ["/auth-plugin"]
