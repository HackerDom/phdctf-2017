#!/bin/bash

service_name=coffeepot

# change directory to the script location
cd "$( dirname "${BASH_SOURCE[0]}")"

docker build -t phdays2017:${service_name}_service ${service_name}_service
