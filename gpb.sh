#!/bin/bash

all_remotes='
origin
github
bitbucket
xosc
'

all_branches='
v1
v2
v3
v4
v5
v6
master
'

if [ -z $1]; then
	branches='master'
elif [ $1 = 'all' ]; then
	branches=$all_branches
else
	branches=$1
fi

echo "Trying to push all branches to the remote repository..."
for rt in $all_remotes; do
	echo "======remote: $rt======"
	for br in $branches; do
		echo "---branch: $br---"
		git push $rt $br
	done
done
