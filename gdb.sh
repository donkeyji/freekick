#!/bin/bash

if [ $1 = '-tui' ]; then
    USE_GUI='-tui'
fi

# ensure that the program could generate a core dump file
ulimit -c unlimited

# the core file's name is determined by /proc/sys/kernel/core_pattern
# the default names vary across ditributions
GDB_OPTIONS="--args"
GDB_BIN=gdb

CORE_FILE=/tmp/*core*
FK_CONF=freekick.conf
FK_BIN=freekick

if [ -f $CORE_FILE ]; then
    $GDB_BIN $USE_GUI $GDB_OPTIONS -c $CORE_FILE $FK_BIN $FK_CONF
else
    $GDB_BIN $USE_GUI $GDB_OPTIONS $FK_BIN $FK_CONF
fi
