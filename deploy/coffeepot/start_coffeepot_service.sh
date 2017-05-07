#!/bin/bash

service_name=coffeepot

docker run --restart unless-stopped --memory=500m --cpu-shares=10 --volumes-from ${service_name}_data --cidfile=/var/run/${service_name}_service.cont_id -d -p 3255:3255 phdays2017:${service_name}_service
