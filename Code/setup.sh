#!/bin/bash

if [ "$EUID" -ne 0 ]
  then echo "Please run as root"
  exit
fi

# Hack to get user caller's directory
HOME_DIR=$(getent passwd $SUDO_USER | cut -d: -f6)

# Put bashrc into a good state
/bin/cp /etc/skel/.bashrc $HOME_DIR

# Change default shell to bash
sudo -H chsh $SUDO_USER -s /bin/bash

# Update apt-get
apt-get -y update
apt-get -y upgrade
echo "updated and upgraded"

# Install apt-get packages
apt-get -y install valgrind
apt-get -y install htop
apt-get -y install nload
apt-get -y install autoconf automake libtool curl make g++ clang unzip
apt-get -y install  libboost-all-dev
apt-get -y install cmake
apt-get -y install build-essential
apt-get -y install re2c
apt-get -y install clang-format
apt-get -y install libspdlog-dev
apt-get -y install libcrypto++-dev
apt-get -y install libjsoncpp-dev
apt-get -y install ripgrep
apt-get -y install python3 python3-pip
apt-get -y install libcrypto++-dev libcrypto++-doc libcrypto++-utils
apt-get -y install nodejs
apt-get -y install ninja-build
apt-get -y remove --auto-remove libprotobuf-dev
apt-get -y remove --auto-remove golang-goprotobuf-dev
apt-get -y remove --auto-remove protobuf-compiler
echo "Installed apt-get packages"

# Install python packages
echo 'export PATH="$PATH:$HOME/bin"' >> $HOME_DIR/.bashrc
echo "export PATH=\"\$PATH:$HOME/.local/bin\"" >> $HOME_DIR/.bashrc
export PATH="$PATH:$HOME_DIR/bin"
pip install numpy
pip install matplotlib
pip install seaborn
pip install plotly
pip install pandas
pip install -U kaleido
echo "Installed Python packages"

# Install nng
wget -q https://github.com/nanomsg/nng/archive/refs/tags/v1.5.2.tar.gz
tar -xzf ./v1.5.2.tar.gz
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
rm -rf ./v1.5.2.tar.gz ./nng-1.5.2 ./build

# protobuf installation
wget -q https://github.com/protocolbuffers/protobuf/releases/download/v3.10.0/protobuf-cpp-3.10.0.tar.gz
tar -xzf protobuf-cpp-3.10.0.tar.gz
cd protobuf-3.10.0
./configure
make -j$(nproc)
make install -j$(nproc)
ldconfig
cd ..
echo "Installed protobuf and protoc"
rm -rf ./protobuf-3.10.0 protobuf-cpp-3.10.0.tar.gz

# Go 1.20 install
wget -q https://go.dev/dl/go1.21.1.linux-amd64.tar.gz
rm -rf /usr/local/go
tar -C /usr/local -xzf go1.21.1.linux-amd64.tar.gz
echo "export PATH=\"\$PATH:/usr/local/go/bin\"" >> $HOME_DIR/.bashrc
export PATH="$PATH:/usr/local/go/bin"
echo "export PATH=\"\$PATH:$HOME/go/bin\"" >> $HOME_DIR/.bashrc
echo "Installed go"
rm -rf go1.21.1.linux-amd64.tar.gz

# Insatall go protoc extension
GOBIN=$HOME_DIR/go/bin go install google.golang.org/protobuf/cmd/protoc-gen-go@latest
echo "Installed go protoc"

# Petty
git config --global core.editor "vim"
echo "export GIT_EDITOR=vim" >> $HOME_DIR/.bashrc
rm -f $HOME_DIR/.tmux.conf
echo "set -g default-terminal \"screen-256color\"" >> $HOME_DIR/.tmux.conf
echo "set-option -g default-shell /bin/bash" >> $HOME_DIR/.tmux.conf


echo "Script is successful!"

# Install algorand js sdk
# cd /proj/ove-PG0/therealmurray/go-algorand/wallet_app
# apt-get -y install npm
# npm install algosdk
