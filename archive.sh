#!/bin/bash

d=$(date "+%Y-%m-%d")
name="hopex-$d.tar.gz"
dst_dir="/Users/huge/Work/VShared/freekick/code/release-hopex"
git archive master --prefix='hopex/' | gzip > $dst_dir/$name
