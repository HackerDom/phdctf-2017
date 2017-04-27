FROM debian:latest
RUN apt-get update && \
    apt-get install -y build-essential gcc make libssl-dev openssl libmysqlclient-dev libc-ares-dev uuid-dev && \
    rm -rf /var/lib/apt/lists/*
COPY mosquitto /usr/src/mosquitto
COPY mosquitto-auth-plug /usr/src/mosquitto-auth-plug
COPY config.mk /usr/src/mosquitto-auth-plug/
WORKDIR /usr/src/mosquitto
RUN make binary
WORKDIR /usr/src/mosquitto-auth-plug
RUN make
