#!/usr/bin/expect

spawn dpkg-reconfigure slapd -freadline

expect "Omit OpenLDAP server configuration?"
send "no\r"

expect "DNS domain name:"
send "iot.phdays.com\r"

expect "Organization name:"
send "phdays\r"

expect "Administrator password:"
send "XfhC57uwby3plBWD\r"

expect "Confirm password:"
send "XfhC57uwby3plBWD\r"

expect "Database backend to use:"
send "2\r"

expect "Do you want the database to be removed when slapd is purged?"
send "no\r"

expect "Move old database?"
send "yes\r"

expect "Allow LDAPv2 protocol?"
send "no\r"

# done
expect eof
