#!/bin/bash
source ldap.cfg
for f in *.ldif
do
	echo "Adding: $f"
	ldapadd -D $LDAP_USER -w $LDAP_PASS -f $f
done
