CREATE DATABASE IF NOT EXISTS mqtt;

USE mqtt;

CREATE USER IF NOT EXISTS 'mqtt_broker'@'%' IDENTIFIED BY 'UFRzFk2Lw5dd9h4u';
CREATE USER IF NOT EXISTS 'thermometer_module'@'%' IDENTIFIED BY '4VS6yKnPF9eLoZkB';

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

GRANT SELECT ON mqtt.users TO 'mqtt_broker'@'%' WITH GRANT OPTION;
GRANT SELECT ON mqtt.acls TO 'mqtt_broker'@'%' WITH GRANT OPTION;

GRANT SELECT,INSERT,UPDATE ON mqtt.users TO 'thermometer_module'@'%' WITH GRANT OPTION;
GRANT INSERT,UPDATE ON mqtt.acls TO 'thermometer_module'@'%' WITH GRANT OPTION;
FLUSH PRIVILEGES;

INSERT INTO acls (username, topic, rw) VALUES ('anonymous', 'house/authorization', 2);
INSERT INTO acls (username, topic, rw) VALUES ('anonymous', 'house/authorization/%c', 1);
INSERT INTO acls (username, topic, rw) VALUES ('temperature-module', 'house/authorization', 1);
INSERT INTO acls (username, topic, rw) VALUES ('temperature-module', 'house/authorization/#', 2);
