# Thermometer service

## About

Thermometer service contains four docker images:
* `thermometer/mqtt-broker` - [Mosquitto](https://mosquitto.org/) MQTT broker and [authentication plugin](https://github.com/jpmens/mosquitto-auth-plug) for it.
* `thermometer/mqtt-db` - MySQL database to store authentication and authorization info for Mosquitto.
* `thermometer/sensor` - Temperature Sensor. It sends current (random) temperature via MQTT broker.
* `thermometer/module` - Temperature Module. It allows to authorize sensor and add new user for MQTT broker. These users can read data from sensors after that.

## Build and deploy

To build and export docker images for service you need to run `./prepare_for_deploy.sh`. To load exported images to docker you can use `docker load` command. To automate this process use `thermometer_service` ansible role from `vuln_image` folder.
