#!/bin/bash

set -e

service slapd stop

rm -vrf /var/lib/ldap
mkdir /var/lib/ldap && chown openldap.openldap /var/lib/ldap

service slapd start

./ldap-dpkg-reconfigure.sh

./ldap-add-locks.sh

./ldap-init.sh

./ldap-search.sh *

echo "`basename $0`: success!"

