#!/bin/bash

if [ -z "$1" ]
then
    echo "Usage: `basename $0` DIR"
    echo
    echo "Copies needed files to vulnbox"
    echo "If DIR does not exist, it will be created"
    echo
    exit 1
fi

if [ ! -d "$1" ]
then
    mkdir -v "$1"
fi

cp -v Makefile doorlock-server.cpp "$1"
cp -v Dockerfile docker-*.sh "$1"
cp -rv libs "$1"
cp -rv include "$1"

mkdir "$1/ldap/"
cp -v ldap/doorlock.schema \
        ldap/ldap-dpkg-reconfigure.sh \
        ldap/ldap-init.sh \
        ldap/ldap.cfg \
        ldap/add-locks.ldif \
        "$1/ldap/"

cp -rv ldap/slapd.d/ "$1/ldap/"

