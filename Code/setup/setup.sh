#!/bin/bash

if [ "$EUID" -ne 0 ]
  then echo "Please run as root"
  exit
fi

# Setup default shell
sudo chsh $SUDO_USER -s /bin/bash

# Update apt
apt -y update
apt -y upgrade
echo "updated and upgraded"

# Install apt packages
apt -y install valgrind
apt -y install htop
apt -y install nload
apt -y install autoconf automake libtool curl make g++ clang unzip
apt -y install  libboost-all-dev
apt -y install cmake
apt -y install build-essential
apt -y install re2c
apt -y install clang-format
apt -y install libspdlog-dev
apt -y install libcrypto++-dev
apt -y install libjsoncpp-dev
apt -y install ripgrep
apt -y install python3 python3-pip
apt -y install libcrypto++-dev libcrypto++-doc libcrypto++-utils
apt -y install nodejs
apt -y install ninja-build
apt -y remove --auto-remove libprotobuf-dev
apt -y remove --auto-remove golang-goprotobuf-dev
apt -y remove --auto-remove protobuf-compiler
echo "Installed apt packages"

# Install python packages
echo "export PATH=\$PATH:\$HOME/.local/bin" >> $HOME/.profile
pip install numpy
pip install matplotlib
pip install seaborn
pip install plotly
pip install pandas
pip install -U kaleido
echo "Installed Python packages"

# Install nng
tar -xzf nng-1.5.2.tar.gz
echo "untar of nng successful"
cd nng-1.5.2
mkdir build
cd build
echo "build directory creation successful"
cmake -G Ninja -S ..
ninja
ninja install
cd ../..
echo "Installed NNG"

# protobuf installation
tar -xzf protobuf-cpp-3.10.0.tar.gz
cd protobuf-3.10.0
./configure
make -j$(nproc)
make install -j$(nproc)
ldconfig
cd ..
echo "Installed protobuf and protoc"

# Go 1.20 install
rm -rf /usr/local/go
tar -C /usr/local -xzf go1.20.1.linux-386.tar.gz
echo "export PATH=\$PATH:/usr/local/go/bin" >> ~/.bashrc
source ~/.bashrc
echo "export PATH=\$PATH:`go env GOPATH`/bin" >> ~/.bashrc
source ~/.bashrc
echo "Installed go"

# Insatall go protoc extension
go install google.golang.org/protobuf/cmd/protoc-gen-go@latest
echo "Installed go protoc"

# Petty
git config --global core.editor "vim"

echo "Script is successful!"

# Install algorand js sdk
# cd /proj/ove-PG0/therealmurray/go-algorand/wallet_app
# apt -y install npm
# npm install algosdk
