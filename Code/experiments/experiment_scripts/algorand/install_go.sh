#!/bin/bash


cd /proj/ove-PG0/murray/go-algorand
sudo rm -rf /usr/local/go
sudo tar -C /usr/local -xzf go1.20.3.linux-amd64.tar.gz
export PATH=$PATH:/usr/local/go/bin
source $HOME/.profile
go version
