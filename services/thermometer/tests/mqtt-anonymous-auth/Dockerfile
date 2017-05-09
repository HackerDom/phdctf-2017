FROM debian:latest

RUN apt-get update && apt-get install -y build-essential gcc make libssl-dev && rm -rf /var/lib/apt/lists/*

COPY paho.mqtt.c /usr/src/paho.mqtt.c
WORKDIR /usr/src/paho.mqtt.c
RUN make && make install

RUN mkdir tests
COPY test.c /usr/src/tests/
WORKDIR /usr/src/tests
RUN gcc -o /usr/bin/test-mqtt-anonymous-auth test.c -l paho-mqtt3c

CMD ["/usr/bin/test-mqtt-anonymous-auth"]
