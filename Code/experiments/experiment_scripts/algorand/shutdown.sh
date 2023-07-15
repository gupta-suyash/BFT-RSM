#!/bin/bash

export ALGORAND_DATA=/proj/ove-PG0/murray/node/n$1
echo $ALGORAND_DATA
~/go/bin/goal node stop
# ~/go/bin/goal kmd stop