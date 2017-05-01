#!/bin/bash
# This is Doorlock API stub
set -e
source ldap.cfg

if [ -z "$3" ]
then
    echo USAGE: `basename $0` LOCK_ID CARD_ID CARD_DATA
    exit 1
fi

LOCK_ID=$1
CARD_ID=$2        # FLAG!
CARD_DATA=$3

LOCK_CN=`ldapsearch -h $LDAP_HOST -D $LDAP_USER -w $LDAP_PASS -b $LDAP_ROOT -LLL lockId=$LOCK_ID | grep cn: | cut -d' ' -f2`
if [ -z $LOCK_CN ]
then
  echo LOCK NOT FOUND
  exit 0
fi

CARD_CN=`< /dev/urandom tr -dc 0-9 | head -c3` # Calc max?

cat << EOF | ldapadd -h $LDAP_HOST -D $LDAP_USER -w $LDAP_PASS
dn: cn=$CARD_CN,cn=$LOCK_CN,cn=locks,$LDAP_ROOT
objectClass: top
objectClass: device
objectClass: cardObject
cardId:      $CARD_ID
cardData:    $CARD_DATA
EOF

echo "Added card (CN=$CARD_CN) to lock $LOCK_ID"
