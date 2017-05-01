FROM alpine:3.4

ENV http_proxy http://xz.avp.ru:8080
ENV https_proxy http://xz.avp.ru:8080

RUN apk add --no-cache mosquitto mariadb-client-libs openssl openssl-dev bash
COPY auth-plug.so /usr/lib/
COPY mosquitto.conf /etc/mosquitto/mosquitto.conf
COPY wait-for-it/wait-for-it.sh /usr/bin/wait-for-it.sh
EXPOSE 1883
