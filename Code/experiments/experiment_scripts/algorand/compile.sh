#!/bin/bash

# Go installation
cd /proj/ove-PG0/murray/go-algorand
sudo rm -rf /usr/local/go
sudo tar -C /usr/local -xzf go1.20.3.linux-amd64.tar.gz
export PATH=$PATH:/usr/local/go/bin
source $HOME/.profile
go version

# Compile algorand
export PATH=$PATH:/usr/local/go/bin
cd /proj/ove-PG0/therealmurray/go-algorand/
./scripts/configure_dev.sh
./scripts/buildtools/install_buildtools.sh
make install

# Do some basic folder setup
