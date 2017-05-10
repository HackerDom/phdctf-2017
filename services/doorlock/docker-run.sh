#!/bin/bash
docker run -d -v ldap:/var/lib/ldap -p 5683:5683/udp doorlock
