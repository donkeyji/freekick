#!/bin/bash

all_remotes=$(git remote)
# the option --no-color is necessary
all_branches=$(git branch --no-color | sed -e '/^  /s/^  //' -e '/^* /s/^* //g')
echo -e "All Branches:\n$all_branches"

if [ -z $1 ]; then
	branches=$(git branch --no-color|grep '*'|sed '/^* /s/^* //')
#elif [ $1 = 'all' ]; then
#	branches=$all_branches
else
	branches=$1
fi

echo -e "Target Branch:\n$branches"

echo "Trying to push all branches to the remote repository..."
for rt in $all_remotes; do
	echo "===================Remote Repository: $rt==================="
	for br in $branches; do
		echo "---------Local Branch: $br---------"
		git push $rt $br
	done
done
