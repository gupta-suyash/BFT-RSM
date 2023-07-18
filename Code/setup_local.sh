#!/bin/bash

if [ "$EUID" -ne 0 ]
  then echo "Please run as root"
  exit
fi

apt-get -y update
apt-get -y upgrade

# install performance packages
apt install valgrind
apt install htop
apt install nload
echo "updated and upgraded"
apt install valgrind
apt install htop
apt install nload
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
apt-get -y install libcrypto++-dev
apt-get -y install libjsoncpp-dev
apt-get -y install ripgrep
apt-get -y install python3 python3-pip
echo "export PATH=\$PATH:\$HOME/.local/bin" >> $HOME/.profile
pip install numpy
pip install matplotlib
pip install seaborn
pip install plotly
pip install pandas
echo "basic packages installed"
apt-get -y install ninja-build
echo "built ninja"
apt-get -y install libcrypto++-dev libcrypto++-doc libcrypto++-utils
apt install nodejs
apt -y install npm

tar -xzf nng-1.5.2.tar.gz
echo "untar of nng successful"
cd nng-1.5.2
mkdir build
cd build
echo "build directory creation successful"
cmake -G Ninja -S ..
ninja
ninja install
cd ..
cd ..

# protobuf installation
tar -xzf protobuf-cpp-3.10.0.tar.gz
echo "untar of protobuf successful"
cd protobuf-3.10.0
./configure
make clean
make -j$(nproc)
make install -j
ldconfig
cd ..

# Go 1.20 install
wget https://go.dev/dl/go1.20.1.linux-386.tar.gz
rm -rf /usr/local/go
tar -C /usr/local -xzf go1.20.1.linux-386.tar.gz
echo "export PATH=\$PATH:/usr/local/go/bin" >> $HOME/.profile
source $HOME/.profile
echo "export PATH=\$PATH:`go env GOPATH`/bin" >> $HOME/.profile
source $HOME/.profile

pip install -U kaleido

# Insatall go protoc extension
go install google.golang.org/protobuf/cmd/protoc-gen-go@latest
apt install golang-goprotobuf-dev
echo "Script is successful!"

# Install algorand js sdk
cd /proj/ove-PG0/therealmurray/go-algorand/wallet_app
npm install algosdk
