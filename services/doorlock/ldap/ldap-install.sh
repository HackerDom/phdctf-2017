#!/bin/bash
set -e

service slapd stop

cp -v doorlock.schema /etc/ldap/schema/doorlock.schema

LDIF=slapd.d/cn=config/cn=schema/cn={4}doorlock.ldif
cp -v $LDIF /etc/ldap/$LDIF
chown openldap.openldap /etc/ldap/$LDIF

service slapd start

echo Success!
