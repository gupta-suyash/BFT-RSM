#!/bin/bash
#if [ "$EUID" -ne 0 ]
#  then echo "Please run as root"
#  exit
#fi

# install packages
sudo apt-get -y update
sudo apt-get -y upgrade
sudo apt install valgrind
sudo apt install htop
sudo apt install nload
echo "updated and upgraded"
sudo apt-get -y install autoconf automake libtool curl make g++ clang unzip
sudo apt-get -y install  libboost-all-dev
sudo apt-get -y install cmake
sudo apt-get -y install build-essential
sudo apt-get -y install re2c
sudo apt-get -y remove --auto-remove libprotobuf-dev
sudo apt-get -y remove --auto-remove golang-goprotobuf-dev
sudo apt-get -y remove --auto-remove protobuf-compiler
sudo apt-get -y install clang-format
sudo apt-get -y install libspdlog-dev
sudo apt-get install libjsoncpp-dev
sudo apt -y install python3-pip
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
echo "untar of protobuf successful"
cd ./protobuf-3.20.2
sudo ./configure
sudo make -j$(nproc)
sudo make install -j
sudo ldconfig
cd ..

# Go 1.19.5 install
sudo rm -rf /usr/local/go 
sudo tar -C /usr/local -xzf go1.20.linux-amd64.tar.gz
export PATH=$PATH:/usr/local/go/bin
go version
echo "export PATH=$PATH:/usr/local/go/bin" >> $HOME/.profile
echo "export GOPATH=$HOME/go" >> $HOME/.profile
echo "export PATH=$PATH:$GOROOT/bin:$GOPATH/bin" >> $HOME/.profile
source $HOME/.profile

# Insatall go protoc extension
go install google.golang.org/protobuf/cmd/protoc-gen-go@latest

echo "Script is successful!"
