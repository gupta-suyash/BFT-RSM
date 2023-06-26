#!/bin/bash

export ALGORAND_DATA=/proj/ove-PG0/ethanxu/node/n$1
echo $ALGORAND_DATA
# ~/go/bin/goal kmd start
~/go/bin/goal node restart -p "10.10.1.13:4161"
cd /proj/ove-PG0/ethanxu/go-algorand/wallet_app
node /proj/ove-PG0/ethanxu/go-algorand/wallet_app/wallet_test.js /proj/ove-PG0/ethanxu/go-algorand/wallet_app/node$1.json test$1 > wallet$1.log
