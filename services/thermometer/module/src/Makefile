all: module.c
	gcc -fno-stack-protector -z execstack -o module module.c ../paho.mqtt.c/src/MQTTClient.c ../paho.mqtt.c/src/MQTTPacket.c ../paho.mqtt.c/src/MQTTPacketOut.c ../paho.mqtt.c/src/StackTrace.c ../paho.mqtt.c/src/Thread.c ../paho.mqtt.c/src/Log.c ../paho.mqtt.c/src/Messages.c ../paho.mqtt.c/src/LinkedList.c ../paho.mqtt.c/src/Heap.c ../paho.mqtt.c/src/Tree.c ../paho.mqtt.c/src/Socket.c ../paho.mqtt.c/src/SocketBuffer.c ../paho.mqtt.c/src/MQTTPersistence.c ../paho.mqtt.c/src/Clients.c ../paho.mqtt.c/src/MQTTProtocolClient.c ../paho.mqtt.c/src/MQTTProtocolOut.c ../paho.mqtt.c/src/MQTTPersistenceDefault.c ../paho.mqtt.c/src/utf-8.c -I/usr/include/ -I/usr/include/mysql -I../paho.mqtt.c/build/ -I../paho.mqtt.c/src/ -lmicrohttpd -lmysqlclient -luuid
install:
	cp module /usr/bin/
clean:
	rm -f module
