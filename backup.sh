#!/bin/bash

ver=$1

#git push origin master

if [ -z $ver ]; then
	d=$(date "+%Y-%m-%d_%H:%M")
	appendix=$d
else
	d=$(date "+%Y-%m-%d")
	appendix=$d-$ver
fi
name="freekick-$appendix.tar.gz"

dst_dir="/Users/huge/Work/VShared/virginix/code/release-freekick"
test -d "$dst_dir" && git archive master --prefix='freekick/' | gzip > "$dst_dir"/$name

dst_dir="/Volumes/SDCard/X-STARS/release-freekick"
test -d "$dst_dir" && git archive master --prefix='freekick/' | gzip > "$dst_dir"/$name

dst_dir="/Volumes/Nokia/Z-Music/release-freekick"
test -d "$dst_dir" && git archive master --prefix='freekick/' | gzip > "$dst_dir"/$name

bak_dir="/Volumes/SDCard/X-STARS/freekick/"
test -d "$bak_dir" && cd "$bak_dir" && git pull
bak_dir="/Volumes/Nokia/Z-Music/freekick/"
test -d "$bak_dir" && cd "$bak_dir" && git pull
bak_dir="/Volumes/Macintosh/Users/huge/Pictures/freekick/"
test -d "$bak_dir" && cd "$bak_dir" && git pull
