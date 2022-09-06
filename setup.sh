#!/bin/sh

# ninja install
echo "shell command working!"
apt-get update
apt-get upgrade
apt-get -y install cmake
apt -y install build-essential
apt-get -y install re2c
gh repo clone ninja-build/ninja
cd ninja
cmake -Bbuild-cmake
cmake --build build-cmake

# nng installation
tar -xzf ./Code/nng-1.5.2.tar.gz
cd nng-1.5.2
mkdir build
cd build
cmake -G Ninja ..
ninja
ninja test
ninja install
