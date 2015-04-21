#!/bin/bash

git push origin master
d=$(date "+%Y-%m-%d_%H:%M")
name="hopex-$d.tar.gz"
dst_dir="/Users/huge/Work/VShared/freekick/code/release-hopex"
git archive master --prefix='hopex/' | gzip > $dst_dir/$name

bak_dir="/Volumes/SD CARD/X-STARS/hopex/"
cd "$bak_dir"
git pull
bak_dir="/Volumes/NO NAME/Z-Music/hopex/"
cd "$bak_dir"
git pull
bak_dir="/Volumes/Macintosh/Users/huge/Pictures/hopex/"
cd "$bak_dir"
git pull
