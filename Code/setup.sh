#!/bin/sh

# install packages
apt-get update
apt-get upgrade
echo "updated and upgraded"
apt-get install autoconf automake libtool curl make g++ unzip
apt-get install  libboost-all-dev
apt-get -y install cmake
apt -y install build-essential
apt-get -y install re2c
apt-get remove libprotobuf-dev
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
