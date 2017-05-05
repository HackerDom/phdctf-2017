#!/bin/bash

if [ ! -d /var/lib/mysql/mysql ]; then
    mysql_install_db
    chown -R mysql:mysql /var/lib/mysql
fi

trap "mysqladmin shutdown" TERM
mysqld_safe --bind-address=0.0.0.0 &

/wait-for-it.sh -t 0 127.0.0.1:3306 -- mysql < /init-db.sql

wait
