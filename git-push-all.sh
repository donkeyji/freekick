#!/bin/bash

remotes='
origin
github
bitbucket
xosc
'

branches='
v1
v2
v3
v4
v5
v6
v7
master
'

echo "try to push all branched to all the remotes..."
for rt in $remotes; do
	echo "======remote: $rt ======"
	for br in $branches; do
		echo "---branch: $br ---"
		git push $rt $br
	done
done
