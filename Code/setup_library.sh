#!/bin/bash

if [ "$EUID" -ne 0 ]
  then echo "Please run as root"
  exit
fi

# replace with your directory
cd /proj/ove-PG0/murray/BFT-RSM/Code

# nng installation
tar -xzf /proj/ove-PG0/murray/BFT-RSM/Code/nng-1.5.2.tar.gz
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
tar -xzf /proj/ove-PG0/murray/BFT-RSM/Code/protobuf-cpp-3.20.2.tar.gz
echo "untar of protobuf successful"
cd ./protobuf-3.20.2
./configure
make clean
make -j$(nproc)
make install -j
ldconfig
cd ..

echo "Script is successful!"
