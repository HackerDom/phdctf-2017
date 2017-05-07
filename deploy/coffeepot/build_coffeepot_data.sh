#!/bin/bash

service_name=coffeepot

# change directory to the script location
cd "$( dirname "${BASH_SOURCE[0]}")"

docker build -t phdays2017:${service_name}_data ${service_name}_data

echo "Creating named container, please delete old one if you want"
docker run --name ${service_name}_data phdays2017:${service_name}_data bash -c "chown -R ${service_name}:${service_name} /home/${service_name}/pots/"
