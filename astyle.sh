#!/bin/bash
astyle -A3 -t -l -Y -p -H -j -k3 *.c *.h
rm -f *.orig
