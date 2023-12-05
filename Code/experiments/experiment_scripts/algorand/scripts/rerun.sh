#!/bin/bash

## How to call this scripts: ./run.sh <algorand app dir> <algorand script dir> <port> <wallet_name> <relay>

export ALGORAND_DATA=$2/node
echo "###########################################Algorand RUN SCRIPT ARGS START"
echo "Algorand Data Path: ${ALGORAND_DATA}"
echo $0
echo $1
echo $2
echo $3
echo $4
echo "###########################################Algorand RUN SCRIPT ARGS END"

# Start the Algorand service
~/go/bin/goal kmd start
~/go/bin/goal node restart -p $3

# Start the Algorand applications (wallets)
cd $1/wallet_app
if [ $5 = "true" ]; then
    echo "DONE WITH RELAY!"
    exit 1
fi
# TODO: Not sure where the node.json thing is going???
node $1/wallet_app/wallet_test.js $1/wallet_app/node.json $4 > $1/wallet_app/wallet.log
echo "DONE WITH PARTICIPATION!"
