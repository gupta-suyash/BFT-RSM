#!/bin/bash

## How to call this scripts: ./run.sh <algorand script dir> <port> <algorand app dir> <wallet_name>

export ALGORAND_DATA=$1/node
echo "Algorand Data Path: ${ALGORAND_DATA}"

# Remove stale data
rm $1/node/privatenet-v1/crash.*
rm $1/node/privatenet-v1/ledger.*

# Start the Algorand service
~/go/bin/goal kmd start
~/go/bin/goal node restart -p "$2"

# Start the Algorand applications (wallets)
cd $3/go-algorand/wallet_app
echo $2
echo $1
echo node$2$1
# TODO: Not sure where the node.json thing is going???
node $3/go-algorand/wallet_app/client.js $3/go-algorand/wallet_app/node/node.json $4 > $3/go-algorand/wallet_app/node/wallet.log
echo "DONE!"