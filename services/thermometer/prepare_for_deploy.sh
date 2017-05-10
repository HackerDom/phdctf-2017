#!/bin/bash

docker-compose -f docker-compose-compile-binaries.yml up --build
docker-compose -f docker-compose-build-images.yml build

for name in mqtt-db mqtt-broker module sensor; do
  echo Exporting thermometer/$name -> ../../vuln_image/docker_images/$name.tar
  docker image save -o ../../vuln_image/docker_images/$name.tar thermometer/$name

  echo Zipping ../../vuln_image/docker_images/$name.tar
  gzip ../../vuln_image/docker_images/$name.tar
done
