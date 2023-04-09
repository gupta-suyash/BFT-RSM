#!/bin/bash

if [ "$EUID" -ne 0 ]
  then echo "Please run as root"
  exit
fi

# replace with your directory
cd /proj/ove-PG0/reggie/BFT-RSM/Code

# install packages
apt-get -y update
apt-get -y upgrade

# install performance packages
apt install valgrind
apt install htop
apt install nload


apt install valgrind
apt install htop
apt install nload
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
apt-get -y install libjsoncpp-dev
apt install nodejs
apt-get -y install libcrypto++-dev
pip install numpy
pip install matplotlib
pip install seaborn
echo "basic packages installed"
sudo apt-get -y install ninja-build
echo "built ninja"

# nng installation
echo "untar of nng successful"
cd ./nng-1.5.2
cd build
#cmake -G Ninja -S ..
sudo ninja
sudo ninja install
cd ..
cd ..

# protobuf installation
cd /proj/ove-PG0/reggie/BFT-RSM/Code
echo "untar of protobuf successful"
cd ./protobuf-3.20.2
sudo ./configure
sudo make -j$(nproc)
sudo make install -j
sudo ldconfig
cd ..

# Go 1.20 install
sudo wget https://go.dev/dl/go1.20.linux-amd64.tar.gz
sudo rm -rf /usr/local/go
sudo tar -C /usr/local -xzf go1.20.linux-amd64.tar.gz
sudo rm -rf go1.20.linux-amd64.tar.gz
echo "export PATH=\$PATH:/usr/local/go/bin" >> $HOME/.profile
source $HOME/.profile
echo "export PATH=\$PATH:`go env GOPATH`/bin" >> $HOME/.profile
source $HOME/.profile

# Insatall go protoc extension
go install google.golang.org/protobuf/cmd/protoc-gen-go@latest

echo "Script is successful!"
