#!/bin/bash
source ldap.cfg
ldapdelete -r -h 127.0.0.1 -D $LDAP_USER -w $LDAP_PASS "cn=locks,dc=iot,dc=phdays,dc=com"
ldapdelete -r -h 127.0.0.1 -D $LDAP_USER -w $LDAP_PASS "ou=locks,dc=iot,dc=phdays,dc=com"
