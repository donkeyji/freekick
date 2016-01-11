#!/bin/bash

test -f ./v.log && rm -f v.log

#valgrind --tool=memcheck --track-origins=yes --show-reachable=yes --read-var-info=yes --verbose --time-stamp=yes --leak-check=full --log-file=v.log ./freekick ./freekick.conf
valgrind --tool=memcheck --leak-check=full --log-file=v.log ./freekick ./freekick.conf
