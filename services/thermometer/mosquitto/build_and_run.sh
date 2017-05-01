#!/bin/bash

docker build -t mosquitto .
docker run -it --rm --name mosquitto -p 1883:1883 mosquitto
