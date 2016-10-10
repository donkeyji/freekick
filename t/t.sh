#/bin/bash

repeat_cnt=$1

if [ -z $repeat_cnt ]; then
    repeat_cnt=1
fi

echo "testing lua script"
redis-cli --eval t.lua a b , x y

for ((i=0;i<$repeat_cnt;i++)) {
    echo "testing other commands"
    python t.py
}
