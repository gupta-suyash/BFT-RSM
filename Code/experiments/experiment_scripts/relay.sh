#!/bin/bash

export ALGORAND_DATA=/proj/ove-PG0/murray/node/relay
echo $ALGORAND_DATA
~/go/bin/goal node restart -p "10.10.1.11:4161"
