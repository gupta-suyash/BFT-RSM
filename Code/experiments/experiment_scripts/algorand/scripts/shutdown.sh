#!/bin/bash

## How to call this scripts: ./shutdown.sh <algorand script dir>

export ALGORAND_DATA=$1/node
echo $ALGORAND_DATA
~/go/bin/goal kmd stop
~/go/bin/goal node stop
