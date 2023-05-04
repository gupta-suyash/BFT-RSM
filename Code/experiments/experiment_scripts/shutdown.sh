#!/bin/bash

export ALGORAND_DATA=/proj/ove-PG0/murray/node7/n$1
echo $ALGORAND_DATA
~/go/bin/goal node stop
# ~/go/bin/goal kmd stop