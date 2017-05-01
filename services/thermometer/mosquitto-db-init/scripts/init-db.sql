CREATE USER 'mqtt_broker'@'%' IDENTIFIED BY 'UFRzFk2Lw5dd9h4u';
GRANT SELECT ON mqtt.* TO 'mqtt_broker'@'%';
FLUSH PRIVILEGES;

CREATE TABLE users (
  id INTEGER AUTO_INCREMENT,
  username VARCHAR(25) NOT NULL,
  pw VARCHAR(128) NOT NULL,
  super INT(1) NOT NULL DEFAULT 0,
  PRIMARY KEY (id)
);

CREATE UNIQUE INDEX users_username ON users (username);
