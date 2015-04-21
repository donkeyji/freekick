#!/bin/bash

git push origin master
d=$(date "+%Y-%m-%d_%H:%M")
name="hopex-$d.tar.gz"
dst_dir="/Users/huge/Work/VShared/freekick/code/release-hopex"
git archive master --prefix='hopex/' | gzip > $dst_dir/$name

bak_dirs=("/Volumes/SD CARD/X-STARS/hopex/", "/Volumes/NO NAME/Z-Music/hopex/", "/Volumes/Macintosh/Users/huge/Pictures/hopex/")
for d in $bak_dir; do
	test -d "$d" && cd "$d" && git pull
done

