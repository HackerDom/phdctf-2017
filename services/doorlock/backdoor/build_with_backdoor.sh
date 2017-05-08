#!/bin/bash

set -e

tar xvf libcoap-with-backdoor.tgz
pushd libcoap

make clean && make
gcc -c src/commit.c
ar cr .libs/libcoap-1.a commit.o

cp -v .libs/libcoap-1.a ../../libs/

popd
rm -rf libcoap

echo Done.
