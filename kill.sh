#!/bin/bash

pid=$(cat /tmp/freekick.pid)
echo $pid

kill -2 $pid
