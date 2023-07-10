#!/bin/bash

if [ $1 == "0" ]
then
    export ALGORAND_DATA=/proj/ove-PG0/therealmurray/node/four_node/r$2$1
    echo $ALGORAND_DATA
    ~/go/bin/goal node stop -p $3
else
    export ALGORAND_DATA=/proj/ove-PG0/therealmurray/node/four_node/n$2$1 # $1 = node idx
    echo $ALGORAND_DATA
    ~/go/bin/goal kmd stop
    ~/go/bin/goal node stop
fi