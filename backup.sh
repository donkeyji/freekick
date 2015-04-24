#!/bin/bash

ver=$1

git push origin master
d=$(date "+%Y-%m-%d_%H:%M")
appendix=$d-$ver
name="hopex-$appendix.tar.gz"

dst_dir="/Users/huge/Work/VShared/freekick/code/release-hopex"
test -d "$dst_dir" && git archive master --prefix='hopex/' | gzip > "$dst_dir"/$name

dst_dir="/Volumes/SDCard/X-STARS/release-hopex"
test -d "$dst_dir" && git archive master --prefix='hopex/' | gzip > "$dst_dir"/$name

dst_dir="/Volumes/Nokia/Z-Music/release-hopex"
test -d "$dst_dir" && git archive master --prefix='hopex/' | gzip > "$dst_dir"/$name

bak_dir="/Volumes/SDCard/X-STARS/hopex/"
test -d "$bak_dir" && cd "$bak_dir" && git pull
bak_dir="/Volumes/Nokia/Z-Music/hopex/"
test -d "$bak_dir" && cd "$bak_dir" && git pull
bak_dir="/Volumes/Macintosh/Users/huge/Pictures/hopex/"
test -d "$bak_dir" && cd "$bak_dir" && git pull

