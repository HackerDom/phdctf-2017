FROM alpine:3.4

RUN apk add --no-cache make gcc musl-dev libuuid libc6-compat openssl-dev util-linux-dev bash

COPY paho.mqtt.c /usr/src/paho.mqtt.c
WORKDIR /usr/src/paho.mqtt.c
RUN make

COPY src /usr/src/thermometer
WORKDIR /usr/src/thermometer
RUN gcc -o /usr/bin/sensor sensor.c ../paho.mqtt.c/src/MQTTClient.c ../paho.mqtt.c/src/MQTTPacket.c ../paho.mqtt.c/src/MQTTPacketOut.c ../paho.mqtt.c/src/StackTrace.c ../paho.mqtt.c/src/Thread.c ../paho.mqtt.c/src/Log.c ../paho.mqtt.c/src/Messages.c ../paho.mqtt.c/src/LinkedList.c ../paho.mqtt.c/src/Heap.c ../paho.mqtt.c/src/Tree.c ../paho.mqtt.c/src/Socket.c ../paho.mqtt.c/src/SocketBuffer.c ../paho.mqtt.c/src/MQTTPersistence.c ../paho.mqtt.c/src/Clients.c ../paho.mqtt.c/src/MQTTProtocolClient.c ../paho.mqtt.c/src/MQTTProtocolOut.c ../paho.mqtt.c/src/MQTTPersistenceDefault.c ../paho.mqtt.c/src/utf-8.c -luuid -lm -I ../paho.mqtt.c/build/ -I ../paho.mqtt.c/src/
