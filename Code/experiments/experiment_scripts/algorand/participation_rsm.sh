#!/bin/bash

NODE=/proj/ove-PG0/therealmurray/node/dummy_node
NAME=test1

if [ "$1" == "0" ]
then
    export ALGORAND_DATA=$NODE/r$2$1
    echo $ALGORAND_DATA
    ~/go/bin/goal node restart -p $3
    echo "WRONG DONE!"
else
    export ALGORAND_DATA=$NODE/n$2$1 # $1 = node idx
    echo "Algorand Data Path: ${ALGORAND_DATA}"
    ~/go/bin/goal kmd start
    ~/go/bin/goal node restart -p "$3" # $2 = ip:port
    cd /proj/ove-PG0/therealmurray/go-algorand/wallet_app
    echo $2
    echo $1
    echo node$2$1
    node /proj/ove-PG0/therealmurray/go-algorand/wallet_app/wallet_test.js /proj/ove-PG0/therealmurray/go-algorand/wallet_app/node/node$2$1.json $NAME > wallet$2$1.log
    echo "DONE!"
fi
