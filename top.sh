#!/bin/bash

pid=$(cat /tmp/freekick.pid)
echo $pid
sys=$(uname)
if [ $sys = 'linux' ]; then
	opt="-p"
else
	opt="-pid"
fi
top $opt $pid
