#!/bin/bash

## How to call this scripts: ./install.sh <algorand app dir> <algorand script dir> <wallet name>
 
# Go installation
sudo rm -rf /usr/local/go
sudo tar -C /usr/local -xzf go1.20.3.linux-amd64.tar.gz
export PATH=$PATH:/usr/local/go/bin
source $HOME/.profile
go version
export PATH=$PATH:/usr/local/go/bin

# Compile algorand
cd $1/go-algorand/
./scripts/configure_dev.sh
./scripts/buildtools/install_buildtools.sh
make install

# Create node directory
mkdir $2/node/
export ALGORAND_DATA=$2/node/
echo $ALGORAND_DATA
~/go/bin/goal node generatetoken
cp $2/genesis.json $ALGORAND_DATA/genesis.json
cp $2/config.json $ALGORAND_DATA/config.json
mkdir $ALGORAND_DATA/privatenet-v1
expect <<-EOF
    proc abort {} {
        puts "Timeout or EOF\n";
        exit 1
    }
    set timeout 20
    spawn ~/go/bin/goal wallet new $2
    expect {
        "Please choose a password for wallet '$2':"          { send -- "\r" }
        default          abort
    }
    expect {
        "Please confirm the password:"          { send -- "\r" }
        default          abort
    }
    expect {
        "Would you like to see it now? (Y/n):"          { send -- "n" }
        default          abort
    }
EOF
~/go/bin/goal account new -w $2 -f > $ALGORAND_DATA/address.txt