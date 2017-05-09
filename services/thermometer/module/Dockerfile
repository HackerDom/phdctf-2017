FROM debian:latest
RUN apt-get update && apt-get install -y build-essential gcc make libssl-dev libmicrohttpd-dev libmysqlclient-dev uuid-dev && rm -rf /var/lib/apt/lists/*

COPY paho.mqtt.c /usr/src/paho.mqtt.c
WORKDIR /usr/src/paho.mqtt.c
RUN make && make install

COPY src /usr/src/module
WORKDIR /usr/src/module
RUN make && make install

COPY wait-for-it.sh /usr/bin/wait-for-it.sh
RUN chmod +x /usr/bin/wait-for-it.sh

EXPOSE 8888

CMD ["wait-for-it.sh", "-t", "0", "mqtt-broker:1883", "--", "/usr/bin/module"]
