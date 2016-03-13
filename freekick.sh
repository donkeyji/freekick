#!/bin/bash

ACTION=$1
if [ -z $ACTION ]; then
    ACTION='start'
fi

case $ACTION in
    start)
        ./freekick freekick.conf
    ;;
    stop)
        killall freekick
    ;;
    status)
        ps aux|grep freekick
    ;;
esac
