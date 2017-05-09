#!/bin/bash

CHECKER=./doorlock.checker.py
TIMEFMT="\t%E real,\t%U user,\t%S sys"

if [ -z "$1" ]
then
    echo "Usage: `basename $0` HOST [COUNT]"
    exit 1
fi

MAX="${2:-10}"
ITER=0
while [ 1 ]
do
    if [ "$ITER" -eq "$MAX" ]; then break; fi

    echo " ========================== $ITER ========================== "
    ITER=$((ITER+1))

    /usr/bin/time -qf "$TIMEFMT" $CHECKER check $1
    CODE=$?
    echo ">>> check: code=$CODE"
    if [ $CODE -ne 101 ]; then continue; fi

    FLAG=`< /dev/urandom tr -dc A-Z | head -c31`=

    FLAG_ID=`/usr/bin/time -qf "$TIMEFMT" $CHECKER put $1 FAKE_ID $FLAG`
    CODE=$?
    echo ">>> put: code=$CODE"
    if [ $CODE -ne 101 ]; then continue; fi

    FLAG_ID=`/usr/bin/time -qf "$TIMEFMT" $CHECKER get $1 $FLAG_ID $FLAG`
    CODE=$?
    echo ">>> get: code=$CODE"
    if [ $CODE -ne 101 ]; then continue; fi
done
