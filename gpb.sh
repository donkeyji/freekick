#!/bin/bash

all_remotes=$(git remote)
# the option --no-color is necessary
all_branches=$(git branch --no-color | sed -e '/^  /s/^  //' -e '/^* /s/^* //g')
echo -e "All Branches Available:\n\033[35m$all_branches\033[0m"

if [ -z $1 ]; then
	branches=$(git branch --no-color|grep '*'|sed '/^* /s/^* //')
#elif [ $1 = 'all' ]; then
#	branches=$all_branches
else
	branches=$1
fi

echo -e "Target Branch: \033[32m$branches\033[0m"
read -p "Wanna Continue? [y/n]" choice
if [ $choice != 'y' ]; then
	echo -e "\033[33mPushing Canled\033[0m"
	exit 1
fi

echo -e "Trying to push all branches to the remote repository..."
for rt in $all_remotes; do
	echo -e "===================Remote Repository: \033[31m$rt\033[0m==================="
	for br in $branches; do
		echo -e "---------Local Branch: \033[36m$br\033[0m---------"
		git push $rt $br
	done
done
