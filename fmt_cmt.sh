#!/bin/bash

f='*.c *.h'

sed -i ''   '/\/\*[^[:space:]]/s#\/\*\([^[:space:]]\)#/* \1#' $f

sed -i ''   '/[^[:space:]]\*\//s#\([^[:space:]]\)\*\/#\1 */#' $f
