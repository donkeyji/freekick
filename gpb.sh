#!/bin/bash

all_remotes=$(git remote)
#all_remotes='
#origin
#github
#bitbucket
#osc
#'

all_branches=$(git branch)
#all_branches='
#v1
#v2
#v3
#v4
#v5
#v6
#master
#'

if [ -z $1 ]; then
	branches='master'
elif [ $1 = 'all' ]; then
	branches=$all_branches
else
	branches=$1
fi

echo "Trying to push all branches to the remote repository..."
for rt in $all_remotes; do
	echo "===================Remote Repository: $rt==================="
	for br in $branches; do
		echo "---------Local Branch: $br---------"
		git push $rt $br
	done
done
