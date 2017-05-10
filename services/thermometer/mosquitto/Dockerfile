FROM alpine:3.4

RUN apk add --no-cache mosquitto mariadb-client-libs openssl openssl-dev bash
COPY mosquitto.conf /etc/mosquitto/mosquitto.conf
COPY auth-plug.so /usr/lib/

COPY wait-for-it.sh /usr/bin/wait-for-it.sh
RUN chmod +x /usr/bin/wait-for-it.sh

EXPOSE 1883

CMD ["wait-for-it.sh", "-t", "0", "mqtt-db:3306", "--", "mosquitto", "-v", "-c", "/etc/mosquitto/mosquitto.conf"]
