clear; clear
make clean
protoc -I./system/protobuf --cpp_out=./system ./system/protobuf/*
make
