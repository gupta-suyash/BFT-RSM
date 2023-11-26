#!/bin/bash

## How to call this scripts: ./run.sh <algorand app dir> <algorand script dir> <port> <wallet_name> <relay>

export ALGORAND_DATA=$1/node
echo "###########################################Algorand RUN SCRIPT ARGS START"
echo "Algorand Data Path: ${ALGORAND_DATA}"
echo $0
echo $1
echo $2
echo $3
echo $4
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
echo "DONE!"
