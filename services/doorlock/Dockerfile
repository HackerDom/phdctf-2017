FROM ubuntu:16.04

LABEL maintainer "dimmoborgir@hackerdom.ru"

WORKDIR /app

RUN apt-get update

RUN DEBIAN_FRONTEND=noninteractive apt-get -y install expect libldap-2.4-2 slapd

# App sources / build requirements
COPY Makefile .
COPY doorlock-server.cpp .
COPY libs libs
COPY include include

COPY ldap/ldap-dpkg-reconfigure.sh .
RUN /app/ldap-dpkg-reconfigure.sh

COPY ldap/doorlock.schema /etc/ldap/schema/doorlock.schema
COPY ldap/slapd.d/cn=config/cn=schema/cn={4}doorlock.ldif /etc/ldap/slapd.d/cn=config/cn=schema/
RUN chown openldap.openldap /etc/ldap/slapd.d/cn=config/cn=schema/cn={4}doorlock.ldif

RUN apt-get -y install ldap-utils

COPY ldap/ldap-init.sh .
COPY ldap/ldap.cfg .
COPY ldap/add-locks.ldif .

COPY docker-wrapper.sh .

RUN apt-get -y install g++ make libldap2-dev
RUN make

RUN useradd doorlock
RUN chown -R doorlock.doorlock /app
USER doorlock

CMD /app/docker-wrapper.sh

EXPOSE 5683/udp
