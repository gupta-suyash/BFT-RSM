#!/bin/bash

# install packages
apt-get -y update
apt-get -y upgrade
echo "updated and upgraded"
apt-get -y install autoconf automake libtool curl make g++ unzip
apt-get -y install  libboost-all-dev
apt-get -y install cmake
apt-get -y install build-essential
apt-get -y install re2c
apt-get -y remove libprotobuf-dev
apt-get -y install golang-goprotobuf-dev
apt-get -y install clang-format
apt-get -y install libspdlog-dev
echo "basic packages installed"

# ninja install
sudo apt-get -y install ninja-build
#git clone https://github.com/ninja-build/ninja
#echo "attempt to clone ninja successful"
#cd ninja
#cmake -Bbuild-cmake
#cmake --build build-cmake
#cd ..
#echo $PWD
echo "built ninja"

# nng installation
tar -xzf ./nng-1.5.2.tar.gz
echo "untar of nng successful"
cd ./nng-1.5.2
mkdir build
cd build
echo "build directory creation successful"
cmake -G Ninja -S ..
ninja
ninja test
ninja install
cd ..
cd ..

# protobuf installation
tar -xzf ./protobuf-cpp-3.20.2.tar.gz
echo "untar of protobuf successful"
cd ./protobuf-3.20.2
./configure
make -j$(nproc)
make check
sudo make install
sudo ldconfig
cd ..
echo "script is successful!"

# Go 1.19.3 install
curl -OL https://golang.org/dl/go1.19.3.linux-amd64.tar.gz
rm -rf /usr/local/go && tar -C /usr/local -xzf go1.19.3.linux-amd64.tar.gz
echo "export PATH=$PATH:/usr/local/go/bin" >> $HOME/.profile
source $HOME/.profile
rm go1.19.3.linux-amd64.tar.gz
go install google.golang.org/protobuf/cmd/protoc-gen-go@latest
