#!/bin/bash

[ -f /var/lib/ldap/DB_CONFIG ] || /app/ldap-dpkg-reconfigure.sh  # to populate 'ldap' volume

/usr/sbin/slapd -h ldap://127.0.0.1:389/ -g openldap -u openldap -F /etc/ldap/slapd.d -d Config &

while ! nc -z 127.0.0.1 389
do
    sleep 1
    echo "Waiting for slapd ... "
done

/app/ldap-init.sh

/app/doorlock-server

