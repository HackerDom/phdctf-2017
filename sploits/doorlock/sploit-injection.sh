#!/bin/bash

if [ -z "$1" ]
then
    echo "Usage: `basename $0` HOST [START_ID]"
    echo
    echo "On the first run, do not pass START_ID"
    echo "On next runs, pass last seen ID as START_ID"
    exit 1
fi

for id in `seq $2 999`
do
    FLAG=`coap -T get "coap://127.0.0.1/get_card?lock=_&card=$id)(%26))"`
    echo $id: $FLAG
    if [[ $FLAG == *EMPTY* ]]
    then
        break
    fi
done
