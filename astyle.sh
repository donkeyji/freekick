#!/bin/bash

files='*.c *.h tmod/*.c'

options='-A10 -t -l -Y -m0 -p -H -j -k3 -n'

astyle $options $files
