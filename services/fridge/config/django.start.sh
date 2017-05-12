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

# DON'T RUN IT IN PRODUCTION. SOME EVIL GUYS CAN BRUTEFORCE PASSWORD AND WHO KNOW WHAT HAPPENS...
echo "[+] [DEBUG] Django setup, executing: add superuser"
PGPASSWORD=${POSTGRES_PASSWORD} psql -U ${POSTGRES_USER} -h ${POSTGRES_HOST} -c "INSERT INTO auth_user (password, last_login, is_superuser, username, first_name, last_name, email, is_staff, is_active, date_joined) VALUES ('pbkdf2_sha256\$36000\$k36V24q60mNo\$v5og9qcgc2sqkVwGjZDKNK+wcJy60ix8DIt9E8Yg48c=', '1970-01-01 00:00:00.000000', true, 'admin', 'admin', 'admin', 'admin@admin', true, true, '1970-01-01 00:00:00.000000') ON CONFLICT (username) DO NOTHING"

# Django: collectstatic
#
echo "[+] Django setup, executing: collectstatic"
python3 /srv/manage.py collectstatic --noinput -v 3

#####
# Start uWSGI
#####
echo "[+] Starting uWSGI..."
uwsgi --emperor /etc/uwsgi/fridge.ini