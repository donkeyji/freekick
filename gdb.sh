#!/bin/bash

# ensure that the program could generate a core dump file
ulimit -c unlimited

# the core file's name is determined by /proc/sys/kernel/core_pattern
# the default names vary across ditributions
CORE_FILE=/tmp/*core*
FK_CONF=freekick.conf
FK_BIN=freekick

if [ -f $CORE_FILE ]; then
    gdb -c $CORE_FILE --args $FK_BIN $FK_CONF
else
    gdb --args $FK_BIN $FK_CONF
fi
