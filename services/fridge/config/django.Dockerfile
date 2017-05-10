FROM ubuntu:latest

MAINTAINER andgein@yandex.ru

RUN apt-get update
RUN apt-get install -y python3 python3-pip uwsgi uwsgi-plugin-python3 postgresql-client

# Install application requirements
ADD ./web/requirements.txt /
RUN pip3 install -U pip
RUN pip3 install -Ur /requirements.txt

# Add code
ADD ./web /srv

# Add start script
ADD ./config/django.start.sh /
RUN chmod +x ./django.start.sh

# Add uWSGI config
ADD ./config/django.uwsgi.ini /etc/uwsgi/fridge.ini

# Add database check script
ADD ./config/db.check.py /

# Execute start script
CMD ["./django.start.sh"]