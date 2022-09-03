#!/bin/bash

pid=$(cat /tmp/freekick.pid)
echo $pid

sys=$(uname)
echo $sys

if [ $sys = 'Linux' ]; then
    opt="-p"
else
    opt="-pid"
fi

top $opt $pid
