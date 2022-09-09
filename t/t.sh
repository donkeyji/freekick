#/bin/bash

repeat_cnt=$1

if [ -z $repeat_cnt ]; then
    repeat_cnt=1
fi

echo "testing lua script"
redis-cli --eval t.lua key1 key2 , value1 value2

#for ((i=0;i<$repeat_cnt;i++)) {
#    echo "testing other commands"
#    python t.py
#}
