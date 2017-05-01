#!/bin/bash
source ldap.cfg
ldapdelete -r -h $LDAP_HOST -D $LDAP_USER -w $LDAP_PASS "cn=locks,dc=iot,dc=phdays,dc=com"
