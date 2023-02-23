#!/bin/bash

# install performance packages
apt install valgrind
apt install htop
apt install nload

# install packages
apt-get -y update
apt-get -y upgrade
echo "updated and upgraded"
apt-get -y install autoconf automake libtool curl make g++ clang unzip
apt-get -y install  libboost-all-dev
apt-get -y install cmake
apt-get -y install build-essential
apt-get -y install re2c
apt-get -y remove --auto-remove libprotobuf-dev
apt-get -y remove --auto-remove golang-goprotobuf-dev
apt-get -y remove --auto-remove protobuf-compiler
apt-get -y install clang-format
apt-get -y install libspdlog-dev
apt-get install libjsoncpp-dev
echo "basic packages installed"

# ninja install
apt-get -y install ninja-build
echo "built ninja"

# nng installation
tar -xzf /proj/ove-PG0/murray/Scrooge/Code/nng-1.5.2.tar.gz
echo "untar of nng successful"
cd ./nng-1.5.2
mkdir build
cd build
echo "build directory creation successful"
cmake -G Ninja -S ..
ninja
ninja install
cd ..
cd ..

# protobuf installation
tar -xzf /proj/ove-PG0/murray/Scrooge/Code/protobuf-cpp-3.20.2.tar.gz
echo "untar of protobuf successful"
cd ./protobuf-3.20.2
./configure
make -j$(nproc)
make install -j
ldconfig
cd ..

# Go 1.19.5 install
wget https://go.dev/dl/go1.20.linux-amd64.tar.gz
rm -rf /usr/local/go
tar -C /usr/local -xzf go1.20.linux-amd64.tar.gz
export PATH=$PATH:/usr/local/go/bin
go version
echo "export PATH=$PATH:/usr/local/go/bin" >> $HOME/.profile
echo "export GOPATH=$HOME/go" >> $HOME/.profile
echo "export PATH=$PATH:$GOROOT/bin:$GOPATH/bin" >> $HOME/.profile
source $HOME/.profile

# Insatall go protoc extension
go install google.golang.org/protobuf/cmd/protoc-gen-go@latest

echo "Script is successful!"
