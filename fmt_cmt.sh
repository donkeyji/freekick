#!/bin/bash

f=$1

sed -i ''   '/\/\*[^[:space:]]/s#\/\*\([^[:space:]]\)#/* \1#' $f

sed -i ''   '/[^[:space:]]\*\//s#\([^[:space:]]\)\*\/#\1 */#' $f
