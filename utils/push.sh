#!/bin/sh

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
    # get the current branch
    cur_branch=$(git branch --no-color|grep '*'|sed '/^* /s/^* //')
else
    cur_branch=$1
fi

echo -e "Target Branch: ${COLOR_GREEN}$cur_branch${COLOR_END}"
illegal=0
while [ $illegal -eq 0 ]; do
    read -p "Proceed? [y/n]: " choice
    echo "Your choice: $choice"
    # we should use double quote for $choice here, in case $choice is null when
    # pressing enter without inputting any letter.
    if [ "$choice" = 'y' -o "$choice" = 'n' ]; then
        illegal=1
        if [ "$choice" = 'n' ]; then
            echo -e "${COLOR_YELLOW}Pushing Canled${COLOR_END}"
            exit 1
        fi
    else
        illegal=0
        echo "Illegal choice, input again"
    fi
done

echo -e "Trying to push the current branch to the remote repository..."
for rt in $all_remotes; do
    echo -e "===================Remote Repository: ${COLOR_RED}$rt${COLOR_END}==================="
    echo -e "---------Local Branch: ${COLOR_CYAN}$cur_branch${COLOR_END}--------"
    git push $rt $cur_branch
done
