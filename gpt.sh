#!/bin/bash

all_remotes=$(git remote)

for r in $all_remotes; do
	echo "=====Pushing Tags To Remote Repository: $r===="
	git push --tags $r
done
