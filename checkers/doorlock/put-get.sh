#!/bin/bash
#
# REQUIRED: npm install coap-cli -g
#

MODEL=`< /dev/urandom tr -dc A-Z | head -c4`
FLOOR=`< /dev/urandom tr -dc 1-9 | head -c1`
ROOM=$FLOOR`< /dev/urandom tr -dc 0-9 | head -c2`

LOCK_ID=`echo -n | coap post "coap://127.0.0.1/register_lock?model=$MODEL&floor=$FLOOR&room=$ROOM"`
echo "LOCK_ID: $LOCK_ID"

CARD_ID=`< /dev/urandom tr -dc 1-3 | head -c1`
CARD_TAG=TAG_`< /dev/urandom tr -dc A-Z | head -c31`=  # FLAG

echo -n | coap post "coap://127.0.0.1/add_card?lock=$LOCK_ID&card=$CARD_ID&tag=$CARD_TAG"

FLAG2=`coap get "coap://127.0.0.1/get_card?lock=$LOCK_ID&card=$CARD_ID"`

if [ "$FLAG2" == "$CARD_TAG" ]
then
    echo "OK (flag found: $CARD_TAG)"
else
    echo "ERROR (expected: '$CARD_TAG', but found: '$FLAG2')"
fi

