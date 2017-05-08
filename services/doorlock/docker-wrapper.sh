#!/bin/bash

/usr/sbin/slapd -h ldap:/// ldapi:/// -g openldap -u openldap -F /etc/ldap/slapd.d -d Any
status=$?
if [ $status -ne 0 ]; then
  echo "Failed to start slapd: $status"
  exit $status
fi
echo Started slapd

/app/doorlock-server
status=$?
if [ $status -ne 0 ]; then
  echo "Failed to start doorlock-server: $status"
  exit $status
fi
echo Started doorlock-server

# Naive check runs checks once a minute to see if either of the processes exited.
# This illustrates part of the heavy lifting you need to do if you want to run
# more than one service in a container. The container will exit with an error
# if it detects that either of the processes has exited.
while /bin/true; do
  PROCESS_1_STATUS=$(ps aux |grep -q slapd)
  PROCESS_2_STATUS=$(ps aux |grep -q doorlock-server)
  if [ $PROCESS_1_STATUS || $PROCESS_2_STATUS ]; then
    echo "One of the processes has already exited."
    exit -1
  fi
  sleep 60
done
