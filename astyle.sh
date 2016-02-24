#!/bin/bash

files=$1
if [ -z $files ]; then
	files='*.c *.h tmod/*.c'
fi

options='-A10 -t -l -Y -m0 -p -H -j -k3 -n'

astyle $options $files
