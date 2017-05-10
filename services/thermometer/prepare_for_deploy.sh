#!/bin/bash

docker-compose -f docker-compose-compile-binaries.yml up --build
docker-compose -f docker-compose-build-images.yml build

mkdir -p ../../vuln_image/docker_images

for name in mqtt-db mqtt-broker module sensor; do
  echo Exporting thermometer/$name -> ../../vuln_image/docker_images/$name
  docker image save -o ../../vuln_image/docker_images/$name thermometer/$name

  echo Zipping ../../vuln_image/docker_images/$name -> ../../vuln_image/docker_images/$name.tgz
  gzip -f -S .tgz ../../vuln_image/docker_images/$name 
done
