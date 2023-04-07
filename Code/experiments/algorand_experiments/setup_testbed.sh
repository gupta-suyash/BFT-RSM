#!/bin/bash

mkdir ~/node/
cd ~/node/
mkdir testnetdata/
export ALGORAND_DATA=~/node/testnetdata
source $HOME/.profile
~/go/bin/goal node generatetoken
cp /proj/ove-PG0/murray/BFT-RSM/Code/experiments/experiment_json/testbed_gensis.json ~/node/testnetdata/genesis.json
mkdir ~/node/testnetdata/privatenet-v1
~/go/bin/goal kmd start -t 3600
~/go/bin/goal wallet new test_wallet
~/go/bin/goal account new -w test_wallet -f

