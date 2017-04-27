#!/bin/bash

docker build -t mosquitto-auth-plugin .
docker run --rm -it -v ${PWD}/../mosquitto:/working_dir mosquitto-auth-plugin cp /usr/src/mosquitto-auth-plug/auth-plug.so /working_dir 
