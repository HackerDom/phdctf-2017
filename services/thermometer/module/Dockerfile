FROM alpine:3.4

RUN apk add --no-cache libuuid libc6-compat libmicrohttpd mariadb-client-libs bash

COPY module /usr/bin/

COPY 01-disable-aslr.conf /etc/sysctl.d/01-disable-aslr.conf

COPY wait-for-it.sh /usr/bin/wait-for-it.sh
RUN chmod +x /usr/bin/wait-for-it.sh

EXPOSE 8888

CMD ["wait-for-it.sh", "-t", "0", "mqtt-broker:1883", "--", "/usr/bin/module"]
