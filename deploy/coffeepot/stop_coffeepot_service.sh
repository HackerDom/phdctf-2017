#!/bin/bash

service_name=coffeepot

if [ ! -f  /var/run/${service_name}_service.cont_id ]; then
 echo "Service is not running"
 exit 1
fi

id=$(cat /var/run/${service_name}_service.cont_id)
docker stop "${id}" && rm /var/run/${service_name}_service.cont_id
