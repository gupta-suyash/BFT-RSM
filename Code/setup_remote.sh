#!/bin/bash

if [ "$EUID" -ne 0 ]
  then echo "Please run as root"
  exit
fi

# replace with your directory
#cd /proj/ove-PG0/suyash/BFT-RSM/Code

# apt-get -y update
# apt-get -y upgrade

# install performance packages
# apt install valgrind
# apt install htop
# apt install nload
# echo "updated and upgraded"
# apt install valgrind
# apt install htop
# apt install nload
# apt-get -y install autoconf automake libtool curl make g++ clang unzip
# apt-get -y install  libboost-all-dev
# apt-get -y install cmake
# cmake -DCMAKE_BUILD_TYPE="Release" .
# apt-get -y install build-essential
# apt-get -y install re2c
# apt-get -y remove --auto-remove libprotobuf-dev
# apt-get -y remove --auto-remove golang-goprotobuf-dev
# apt-get -y remove --auto-remove protobuf-compiler
# apt-get -y install clang-format
# apt-get -y install libspdlog-dev
# apt-get -y install libcrypto++-dev
# apt-get -y install libjsoncpp-dev
# apt-get -y install ripgrep
# apt-get -y install python3 python3-pip
# echo "export PATH=\$PATH:\$HOME/.local/bin" >> $HOME/.profile
# pip install numpy
# pip install matplotlib
# pip install seaborn
# pip install plotly
# pip install pandas
# echo "basic packages installed"
# apt-get -y install ninja-build
# echo "built ninja"
# apt-get -y install libcrypto++-dev libcrypto++-doc libcrypto++-utils
# apt -y install nodejs
# apt -y install npm
cd /proj/ove-PG0/therealmurray/go-algorand/wallet_app
npm install algosdk
echo "Apt script is successful!"

# echo "untar of protobuf successful"
# cd /proj/ove-PG0/therealmurray/BFT-RSM/Code/protobuf-3.10.0
# ./configure
# make install -j
# ldconfig
# cd ..
# echo "Protobuf install is good"