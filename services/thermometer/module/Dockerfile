FROM alpine:3.4

ENV http_proxy http://xz.avp.ru:8080
ENV https_proxy http://xz.avp.ru:8080
ENV no_proxy 127.0.0.1,localhost

RUN apk add --no-cache gcc make musl-dev libmicrohttpd libmicrohttpd-dev curl
RUN mkdir /usr/src

COPY src /usr/src/module
WORKDIR /usr/src/module
RUN make && make install

EXPOSE 8888

CMD ["/usr/bin/server"]
