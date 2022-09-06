#!/bin/sh

# ninja install
echo "shell command starting!"
apt-get update
apt-get upgrade
echo "shell updated and upgraded"
apt-get install  libboost-all-dev
apt-get -y install cmake
apt -y install build-essential
apt-get -y install re2c
echo "basic packages installed"
git clone https://github.com/ninja-build/ninja
echo "attempt to clone ninja successful"
cd ninja
cmake -Bbuild-cmake
cmake --build build-cmake
cd ..
echo $PWD
echo "built ninja"

# nng installation
tar -xzf ./Code/nng-1.5.2.tar.gz
echo "untar of nng successful"
cd ./nng-1.5.2
mkdir build
cd build
echo "build directory creation successful"
cmake -G Ninja ..
ninja
ninja test
ninja install
cd ..
echo "script is successful!"
