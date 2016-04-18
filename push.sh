#!/bin/bash

# color definition
COLOR_END="\033[0m"
COLOR_RED="\033[31m"
COLOR_GREEN="\033[32m"
COLOR_YELLOW="\033[33m"
COLOR_BLUE="\033[34m"
COLOR_PURPLE="\033[35m"
COLOR_CYAN="\033[36m"
COLOR_WHITE="\033[37m"

all_remotes=$(git remote)
# the option --no-color is necessary
all_branches=$(git branch --no-color | sed -e '/^  /s/^  //' -e '/^* /s/^* //g')
echo -e "All Branches Available:\n${COLOR_PURPLE}${all_branches}${COLOR_END}"

if [ -z $1 ]; then
    branches=$(git branch --no-color|grep '*'|sed '/^* /s/^* //')
#elif [ $1 = 'all' ]; then
#   branches=$all_branches
else
    branches=$1
fi

echo -e "Target Branch: ${COLOR_GREEN}$branches${COLOR_END}"
read -p "Wanna Continue? [y/n]: " choice
if [ $choice != 'y' ]; then
    echo -e "${COLOR_YELLOW}Pushing Canled${COLOR_END}"
    exit 1
fi

echo -e "Trying to push all branches to the remote repository..."
for rt in $all_remotes; do
    echo -e "===================Remote Repository: ${COLOR_RED}$rt${COLOR_END}==================="
    for br in $branches; do
        echo -e "---------Local Branch: ${COLOR_CYAN}$br${COLOR_END}--------"
        git push $rt $br
    done
done
