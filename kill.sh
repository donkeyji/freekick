#!/bin/bash

if [ -f /tmp/freekick.pid ]; then
    echo "pidfile exists, to use kill -TERM"
    pid=$(cat /tmp/freekick.pid)
    echo "pid: $pid"
    kill -TERM $pid
else
    echo "pidfile does not exists, to use killall -TERM"
    killall -TERM freekick
fi

