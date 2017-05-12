FROM ubuntu:16.04
MAINTAINER bay@hackerdom.ru

RUN useradd coffeepot

ADD coffeepot /home/coffeepot
RUN chown -R coffeepot:coffeepot /home/coffeepot/pots
RUN chmod +x /home/coffeepot/coffee_httpd
RUN chmod +x /home/coffeepot/coffeepot.cgi
RUN chmod -R +r /home/coffeepot

WORKDIR /home/coffeepot
USER coffeepot

CMD /home/coffeepot/coffee_httpd 0.0.0.0:3255

EXPOSE 3255