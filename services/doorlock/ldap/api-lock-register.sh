#!/bin/bash
# This is Doorlock API stub
set -e
source ldap.cfg

LOCK_ID=`< /dev/urandom tr -dc 0-9 | head -c14`
LOCK_MODEL=`< /dev/urandom tr -dc A-Z | head -c4`

echo Generated random:
echo "LOCK_ID:    $LOCK_ID"
echo "LOCK_MODEL: $LOCK_MODEL"

cat << EOF | ldapadd -h $LDAP_HOST -D $LDAP_USER -w $LDAP_PASS
dn: cn=$LOCK_ID,cn=locks,$LDAP_ROOT
objectClass: top
objectClass: device
objectClass: lockObject
lockModel:   Secure Lock Model $LOCK_MODEL
timestamp:   199412161032Z
EOF

echo "Created new lock with lockId: $LOCK_ID"
