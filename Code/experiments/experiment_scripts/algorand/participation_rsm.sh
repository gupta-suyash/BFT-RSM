#!/bin/bash

if [ $1 == "-1" ]
then
    export ALGORAND_DATA=/proj/ove-PG0/therealmurray/node/four_node/r0$2
    echo $ALGORAND_DATA
    ~/go/bin/goal node restart -p $3
else
    export ALGORAND_DATA=/proj/ove-PG0/therealmurray/node/four_node/n$2$1 # $1 = node idx
    echo $ALGORAND_DATA
    ~/go/bin/goal kmd start
    ~/go/bin/goal node restart -p $3 # $2 = ip:port
    cd /proj/ove-PG0/therealmurray/go-algorand/wallet_app
    node /proj/ove-PG0/therealmurray/go-algorand/wallet_app/wallet_test.js /proj/ove-PG0/therealmurray/go-algorand/wallet_app/node/node$1.json test$1 > wallet$1.log
fi
