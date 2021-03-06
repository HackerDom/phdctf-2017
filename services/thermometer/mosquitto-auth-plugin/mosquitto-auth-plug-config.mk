# Select your backends from this list
BACKEND_CDB ?= no
BACKEND_MYSQL ?= yes
BACKEND_SQLITE ?= no
BACKEND_REDIS ?= no
BACKEND_POSTGRES ?= no
BACKEND_LDAP ?= no
BACKEND_HTTP ?= no
BACKEND_JWT ?= no
BACKEND_MONGO ?= no

# Specify the path to the Mosquitto sources here
MOSQUITTO_SRC = /usr/mosquitto

# Specify the path the OpenSSL here
OPENSSLDIR = /usr

# Specify optional/additional linker flags here
LDFLAGS =
