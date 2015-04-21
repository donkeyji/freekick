#!/bin/bash

git push origin master
d=$(date "+%Y-%m-%d_%H:%M")
name="hopex-$d.tar.gz"

dst_dir="/Users/huge/Work/VShared/freekick/code/release-hopex"
mkdir -pv "$dst_dir"
git archive master --prefix='hopex/' | gzip > "$dst_dir"/$name

dst_dir="/Volumes/SD CARD/X-STARS/release-hopex/"
mkdir -pv "$dst_dir"
git archive master --prefix='hopex/' | gzip > "$dst_dir"/$name

dst_dir="/Volumes/NO NAME/Z-Music/release-hopex/"
mkdir -pv "$dst_dir"
git archive master --prefix='hopex/' | gzip > "$dst_dir"/$name

bak_dir="/Volumes/SD CARD/X-STARS/hopex/"
test -d "$bak_dir" && cd "$bak_dir" && git pull
bak_dir="/Volumes/NO NAME/Z-Music/hopex/"
test -d "$bak_dir" && cd "$bak_dir" && git pull
bak_dir="/Volumes/Macintosh/Users/huge/Pictures/hopex/"
test -d "$bak_dir" && cd "$bak_dir" && git pull

