#!/bin/bash

files=$1
if [ -z $files ]; then
    files=$(find . -name "*.c" -o -name "*.h")
fi
echo files

indent_options='-di1 -cd1 --no-tabs -c1 -cd1'
astyle_options='-A10 -s4 -l -Y -m0 -p -H -j -k3 -n'

for f in $files; do
    #indent $indent_options $f
    astyle $astyle_options $f
    #rm -f ${f}.BAK
    #rm -f ${f}~
done
