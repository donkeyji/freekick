#!/bin/bash

files=$*

if [ -z $files ]; then
    files=$(find . -name "*.c" -o -name "*.h")
fi

for f in $files; do
    # /*abc */ ===> /* abc */
    sed -i ''  '/\/\*[^[:space:]]/s#\/\*\([^[:space:]]\)#/* \1#' $f

    # /* abc*/ ===> /* abc */
    sed -i ''  '/[^[:space:]]\*\//s#\([^[:space:]]\)\*\/#\1 */#' $f

    # abc;/* xyz */ ===> abc; /* xyz */
    sed -i ''  '/[^[:space:]]\/\*/s/\([^[:space:]]\)\/\*/\1 \/\*/g' $f
done
