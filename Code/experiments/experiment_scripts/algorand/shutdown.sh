#!/bin/bash
NODE=/proj/ove-PG0/therealmurray/node/dummy_node

if [ $1 == "0" ]
then
    export ALGORAND_DATA=$NODE/r$2$1
    echo $ALGORAND_DATA
    ~/go/bin/goal node stop
else
    export ALGORAND_DATA=$NODE/n$2$1 # $1 = node idx
    echo $ALGORAND_DATA
    ~/go/bin/goal kmd stop
    ~/go/bin/goal node stop
fi