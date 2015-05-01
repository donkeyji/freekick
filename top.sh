#!/bin/bash

pid=$(cat /tmp/freekick.pid)
echo $pid
top -pid $pid
