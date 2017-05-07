#!/bin/bash

#####
# Postgres: wait until container is created
#####

set +e
echo "[+] Checking is postgres container started"
python3 /db.check.py
while [[ $? != 0 ]] ; do
    sleep 5; echo "[*] Waiting for postgres container..."
    python3 /db.check.py
done
set -e

#####
# Django setup
#####

# Django: migrate
#
# Django will see that the tables for the initial migrations already exist
# and mark them as applied without running them. (Django won’t check that the
# table schema match your models, just that the right table names exist).
echo "[+] Django setup, executing: migrate"
python3 /srv/manage.py migrate --fake-initial

echo "[+] Django setup, executing: createcachetable"
python3 /srv/manage.py createcachetable

# Django: collectstatic
#
echo "[+] Django setup, executing: collectstatic"
python3 /srv/manage.py collectstatic --noinput -v 3

#####
# Start uWSGI
#####
echo "[+] Starting uWSGI..."
uwsgi --emperor /etc/uwsgi/fridge.ini