#!/bin/bash

## How to call this scripts: ./run.sh <algorand app dir> <algorand script dir> <port> <wallet_name>

export ALGORAND_DATA=$1/node
echo "###########################################Algorand RUN SCRIPT ARGS START"
echo "Algorand Data Path: ${ALGORAND_DATA}"
echo $0
echo $1
echo $2
echo $3
echo "###########################################Algorand RUN SCRIPT ARGS END"

# Remove stale data
rm $1/node/privatenet-v1/crash.*
rm $1/node/privatenet-v1/ledger.*

# Start the Algorand service
~/go/bin/goal kmd start
~/go/bin/goal node restart -p $2

# Start the Algorand applications (wallets)
cd $0/wallet_app
echo node$2$1
# TODO: Not sure where the node.json thing is going???
node $0/wallet_app/client.js $0/wallet_app/node.json $3 > $0/wallet_app/wallet.log
echo "DONE!"
