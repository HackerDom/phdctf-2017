FROM ubuntu:16.04

WORKDIR /app

ENV DEBIAN_FRONTEND noninteractive

RUN apt-get update
RUN apt-get -y install libldap-2.4-2 slapd

EXPOSE 5683

#ADD libs .
#ADD Makefile .
COPY doorlock-server .
COPY docker-wrapper.sh .

CMD /app/docker-wrapper.sh
