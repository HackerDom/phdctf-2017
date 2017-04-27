FROM debian:latest
RUN apt-get update && apt-get install -y build-essential gcc make libssl-dev && rm -rf /var/lib/apt/lists/*
COPY paho.mqtt.c /usr/src/paho.mqtt.c
WORKDIR /usr/src/paho.mqtt.c
RUN make && make install
COPY src /usr/src/thermometer
WORKDIR /usr/src/thermometer
RUN gcc -o /usr/bin/thermometer thermometer.c -l paho-mqtt3c
CMD ["/usr/bin/thermometer"]
