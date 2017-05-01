#!/bin/bash
# This is Doorlock API stub
set -e
source ldap.cfg

LOCK_ID=`< /dev/urandom tr -dc 0-9 | head -c14`
LOCK_MODEL=`< /dev/urandom tr -dc A-Z | head -c4`
LOCK_FLOOR=`< /dev/urandom tr -dc 1-9 | head -c1`
LOCK_ROOM=$LOCK_FLOOR`< /dev/urandom tr -dc 0-9 | head -c2`

echo Generated random:
echo "LOCK_ID:    $LOCK_ID"
echo "LOCK_MODEL: $LOCK_MODEL"
echo "LOCK_FLOOR: $LOCK_FLOOR"
echo "LOCK_ROOM:  $LOCK_ROOM"

/usr/bin/time -f "\t%E real,\t%U user,\t%S sys" -a -o api-lock-register.log cat << EOF | ldapadd -h $LDAP_HOST -D $LDAP_USER -w $LDAP_PASS
dn: cn=$LOCK_ID,cn=locks,$LDAP_ROOT
objectClass: top
objectClass: device
objectClass: lockObject
lockModel:   Secure Lock Model $LOCK_MODEL
lockFloor:   $LOCK_FLOOR
lockRoom:    $LOCK_ROOM
timestamp:   199412161032Z
EOF

echo "Created new lock with lockId: $LOCK_ID"
