CREATE DATABASE IF NOT EXISTS mqtt;

USE mqtt;

DROP USER IF EXISTS 'mqtt_broker'@'%';
CREATE USER 'mqtt_broker'@'%' IDENTIFIED BY 'UFRzFk2Lw5dd9h4u';
FLUSH PRIVILEGES;

GRANT SELECT ON mqtt.* TO 'mqtt_broker'@'%' WITH GRANT OPTION;
FLUSH PRIVILEGES;

CREATE TABLE IF NOT EXISTS users (
  id INTEGER AUTO_INCREMENT,
  username VARCHAR(25) NOT NULL,
  pw VARCHAR(128) NOT NULL,
  super INT(1) NOT NULL DEFAULT 0,
  PRIMARY KEY (id)
);

CREATE UNIQUE INDEX IF NOT EXISTS users_username ON users (username);

CREATE TABLE IF NOT EXISTS acls (
  id INTEGER AUTO_INCREMENT,
  username VARCHAR(25) NOT NULL,
  topic VARCHAR(256) NOT NULL,
  rw INT(1) NOT NULL DEFAULT 1,
  PRIMARY KEY (id)
);

CREATE UNIQUE INDEX IF NOT EXISTS acls_user_topic ON acls (username, topic(228));

INSERT INTO acls (username, topic, rw) VALUES ('anonymous', 'house/authorization', 2);
