#!/bin/bash

files=$*

if [ -z $files ]; then
    files=$(find . -name "*.c" -o -name "*.h")
fi

for f in $files; do
    sed -i ''   '/\/\*[^[:space:]]/s#\/\*\([^[:space:]]\)#/* \1#' $f

    sed -i ''   '/[^[:space:]]\*\//s#\([^[:space:]]\)\*\/#\1 */#' $f

    sed -i ''  '/[^[:space:]]\/\*/s/\([^[:space:]]\)\/\*/\1 \/\*/g'  $f
done
