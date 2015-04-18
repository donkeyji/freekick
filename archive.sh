#!/bin/bash

d=$(date "+%Y-%m-%d")
name="hopex-$d.tar.gz"
git archive master --prefix='hopex/' | gzip > ~/Work/VShared/freekick/code/hope-release/$name
