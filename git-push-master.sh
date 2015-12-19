#!/bin/bash

remotes='
origin
github
bitbucket
xosc
'

for rt in $remotes; do
	echo "=====pushing master to $rt====="
	git push $rt master
done
