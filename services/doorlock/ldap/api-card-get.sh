#!/bin/bash
# This is Doorlock API stub
set -e
source ldap.cfg

if [ -z "$2" ]
then
    echo USAGE: `basename $0` LOCK_ID CARD_ID
    exit 1
fi

LOCK_ID=$1
CARD_ID=$2

./ldap-search.sh "(&(lockId=$LOCK_ID)(cn=$CARD_ID))"
