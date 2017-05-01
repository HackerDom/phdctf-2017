FROM alpine:3.4

RUN apk add --no-cache mosquitto mariadb-client-libs openssl openssl-dev bash
COPY auth-plug.so /usr/lib/
COPY mosquitto.conf /etc/mosquitto/mosquitto.conf
COPY wait-for-it/wait-for-it.sh /usr/bin/wait-for-it.sh
EXPOSE 1883
CMD ["mosquitto", "-v", "-c", "/etc/mosquitto/mosquitto.conf"]
