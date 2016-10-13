#!/bin/bash

# ensure that the program could generate a core dump file
ulimit -c unlimited

# the core file's name is determined by /proc/sys/kernel/core_pattern
# the default names vary across ditributions
GDB_OPTIONS='-tui --args'
GDB_BIN=gdb

CORE_FILE=/tmp/*core*
FK_CONF=freekick.conf
FK_BIN=freekick

if [ -f $CORE_FILE ]; then
    $GDB_BIN $GDB_OPTIONS -c $CORE_FILE $FK_BIN $FK_CONF
else
    $GDB_BIN $GDB_OPTIONS $FK_BIN $FK_CONF
fi
