#!/bin/bash
service slapd stop
cp -v doorlock.schema /etc/ldap/schema/doorlock.schema
cp -v slapd.d/cn=config/cn=schema/cn={4}doorlock.ldif /etc/ldap/slapd.d/cn=config/cn=schema/cn={4}doorlock.ldif
service slapd start
