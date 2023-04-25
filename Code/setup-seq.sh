# nng installation
#tar -xzf nng-1.5.2.tar.gz
echo "untar of nng successful"
cd /proj/ove-PG0/suyash/BFT-RSM/Code/nng-1.5.2
#mkdir build
cd build
echo "build directory creation successful"
cmake -G Ninja -S ..
ninja
ninja install
cd ..
cd ..

# protobuf installation
#tar -xzf protobuf-cpp-3.10.0.tar.gz
echo "untar of protobuf successful"
cd /proj/ove-PG0/suyash/BFT-RSM/Code/protobuf-3.10.0
./configure
make clean
make -j$(nproc)
make install -j
ldconfig
cd ..

# Go 1.20 install
#sudo wget https://go.dev/dl/go1.20.1.linux-386.tar.gz
sudo rm -rf /usr/local/go
sudo tar -C /usr/local -xzf go1.20.1.linux-386.tar.gz
echo "export PATH=\$PATH:/usr/local/go/bin" >> $HOME/.profile
source $HOME/.profile
echo "export PATH=\$PATH:`go env GOPATH`/bin" >> $HOME/.profile
source $HOME/.profile

# Insatall go protoc extension
go install google.golang.org/protobuf/cmd/protoc-gen-go@latest
