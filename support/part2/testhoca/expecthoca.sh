#!/usr/bin/expect --

set seed "[lrange $argv 0 0]\n"

spawn emacsim hoca -r$seed

expect "(emacsim)" ; send "c\r"
expect "(emacsim)"; send "quit\r"
expect "Quit anyway? (y or n)"; send "y\r"

expect "error: cannot creat" { exit 1 }

