#!/bin/bash

export ALGORAND_DATA=/proj/ove-PG0/ethanxu/node/relay2
echo $ALGORAND_DATA
~/go/bin/goal node restart -p "10.10.1.13:4161"
