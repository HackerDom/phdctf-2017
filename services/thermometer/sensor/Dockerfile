FROM alpine:3.4

RUN apk add --no-cache libuuid libc6-compat bash

COPY sensor /usr/bin/

COPY wait-for-it.sh /usr/bin/wait-for-it.sh
RUN chmod +x /usr/bin/wait-for-it.sh

CMD ["wait-for-it.sh", "-t", "0", "mqtt-broker:1883", "--", "/usr/bin/sensor"]
