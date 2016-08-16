#/bin/bash

echo "testing lua script"
redis-cli --eval t.lua a b , x y

echo "testing other commands"
python t.py
