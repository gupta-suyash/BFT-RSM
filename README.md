# BFT-RSM

![](https://github.com/gupta-suyash/BFT-RSM/workflows/Demo/badge.svg?event=push)


How to make code:
1. Run setup scripts: sudo ./setup.sh
2. protoc -I=./system/protobuf --cpp_out=./system ./system/protobuf/crosschainmessage.proto
3. make

Note: You only need to run steps 1 and 2 once per new machine
