#!/bin/bash

test -f ./v.log && rm -f v.log

TOP_DIR=..

FK_CONF=$TOP_DIR/conf/freekick.conf
FK_BIN=$TOP_DIR/src/freekick

#valgrind --tool=memcheck --track-origins=yes --show-reachable=yes --read-var-info=yes --verbose --time-stamp=yes --leak-check=full --log-file=v.log ./freekick ./freekick.conf
valgrind --tool=memcheck --leak-check=full --show-reachable=yes --log-file=v.log $FK_BIN $FK_CONF
