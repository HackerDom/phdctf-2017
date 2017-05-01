#!/bin/sh
DIR=slapd.d

[ -d $DIR ] && rm -v -r $DIR
mkdir -v $DIR
slaptest -f doorlock.conf -F $DIR
