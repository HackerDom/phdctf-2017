FROM ubuntu:16.04

LABEL maintainer "dimmoborgir@hackerdom.ru"

WORKDIR /app

RUN apt-get update

RUN apt-get --no-install-recommends -y install apt-utils expect

RUN DEBIAN_FRONTEND=noninteractive apt-get --no-install-recommends -y install slapd libldap-2.4-2

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

RUN apt-get --no-install-recommends -y install ldap-utils

COPY ldap/ldap-init.sh .
COPY ldap/ldap.cfg .
COPY ldap/add-locks.ldif .

RUN apt-get --no-install-recommends -y install g++ make libldap2-dev
RUN make

RUN apt-get --no-install-recommends -y install netcat

COPY docker-wrapper.sh .
CMD /app/docker-wrapper.sh

EXPOSE 5683/udp
