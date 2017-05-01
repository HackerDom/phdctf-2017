#!/bin/bash
if [ -z "$1" ]
then
	echo USAGE: $0 QUERY
	exit 1
fi
source ldap.cfg
ldapsearch -b $LDAP_ROOT -s sub -h 127.0.0.1 -D $LDAP_USER -w $LDAP_PASS "$1" -LLL
