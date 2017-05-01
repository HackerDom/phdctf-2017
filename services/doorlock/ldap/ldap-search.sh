#!/bin/bash
if [ -z "$1" ]
then
	echo USAGE: $0 QUERY
	exit 1
fi
source ldap.cfg
ldapsearch -h $LDAP_HOST -D $LDAP_USER -w $LDAP_PASS -b $LDAP_ROOT "$1" -LLL
