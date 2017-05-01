#!/bin/sh
DIR=slapd.d
set -e

[ -d $DIR ] && rm -v -r $DIR
mkdir -v $DIR
slaptest -f doorlock.conf -F $DIR

echo Success!
