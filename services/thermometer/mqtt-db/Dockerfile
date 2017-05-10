FROM alpine:3.4

RUN apk add --no-cache mysql mysql-client bash && rm -rf /var/lib/mysql

ADD start.sh /
RUN chmod +x /start.sh
ADD init-db.sql /

COPY wait-for-it.sh /wait-for-it.sh
RUN chmod +x /wait-for-it.sh

VOLUME ["/var/lib/mysql"]
EXPOSE 3306

CMD [ "/start.sh" ]
