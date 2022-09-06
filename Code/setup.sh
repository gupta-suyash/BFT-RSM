#!/bin/sh

# ninja install
echo "shell command working!"
sudo apt-get update
sudo apt-get upgrade
sudo apt-get -y install cmake
sudo apt -y install build-essential
sudo apt-get -y install re2c
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
