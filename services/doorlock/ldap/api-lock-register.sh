#!/bin/bash
# This is Doorlock API stub
set -e
source ldap.cfg

# Do it once, at service start

MAX_CN=`ldapsearch -h $LDAP_HOST -D $LDAP_USER -w $LDAP_PASS -b $LDAP_ROOT -LLL objectClass=lockObject cn | \
    grep cn: | cut -d' ' -f2 | sort -n | tail -n1`

printf -v DEC_CN "%.f" $MAX_CN
printf -v NEW_CN "%05d" $((DEC_CN+1))

LOCK_ID=`< /dev/urandom tr -dc 0-9 | head -c14`    # TODO: Argument
LOCK_MODEL=`< /dev/urandom tr -dc A-Z | head -c4`  # TODO: Argument

echo Generated random:
echo "LOCK_ID:    $LOCK_ID"
echo "LOCK_MODEL: $LOCK_MODEL"

cat << EOF | ldapadd -h $LDAP_HOST -D $LDAP_USER -w $LDAP_PASS
dn: cn=$NEW_CN,cn=locks,$LDAP_ROOT
objectClass: top
objectClass: device
objectClass: lockObject
lockId:      $LOCK_ID
lockModel:   Secure Lock Model $LOCK_MODEL
EOF

echo "Created new lock with lockId: $LOCK_ID"

