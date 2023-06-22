#! /bin/bash

# Setup script transferred to remote node's home directory and subsequently executed.
# Used only by setup_raft.sh.

set -e # Exit immediately if any command fails

sudo add-apt-repository -y ppa:longsleep/golang-backports
sudo apt update
sudo apt -y upgrade
sudo apt -y install golang-go
echo "export PATH=$PATH:$(go env GOPATH)/bin" >> $HOME/.profile
source $HOME/.profile
export GOPATH=$(go env GOPATH)
mkdir -p ~/go/src/go.etcd.io
cd ~/go/src/go.etcd.io
GIT_SSH_COMMAND='ssh -o StrictHostKeyChecking=no' git clone git@github.com:etcd-io/etcd.git
wait
cd ~/go/src/go.etcd.io/etcd/contrib/raftexample
go build -o raftexample

echo "host: $(hostname -i) finished setup"
