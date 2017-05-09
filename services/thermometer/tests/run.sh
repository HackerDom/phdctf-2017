#!/bin/bash

docker build -t test-mqtt-anonymous-auth mqtt-anonymous-auth
docker run --rm -it --network thermometer_default test-mqtt-anonymous-auth
