FROM alpine:latest

ENV http_proxy http://xz.avp.ru:8080
ENV https_proxy http://xz.avp.ru:8080

RUN apk add --no-cache mysql-client bash
COPY wait-for-it/wait-for-it.sh /usr/bin/wait-for-it.sh
