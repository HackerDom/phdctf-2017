#!/bin/bash

while [ 1 ]
do
    echo -n `date` >> loop.log
    Q=`/usr/bin/time -f "\t%E real,\t%U user,\t%S sys" -a -o loop.log ./put-get.sh`
    echo $Q >> loop.log
done
