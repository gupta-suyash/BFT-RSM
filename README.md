# BFT-RSM

![](https://github.com/gupta-suyash/BFT-RSM/workflows/Demo/badge.svg?event=push)


How to make code:
1. Run setup scripts: sudo ./setup.sh
2. protoc -I=./system/protobuf --cpp_out=./system ./system/protobuf/crosschainmessage.proto
3. make

Note: You only need to run steps 1 and 2 once per new machine

To run the program (assuming only 2 clusters as of right now): ./scrooge /proj/ove-PG0/murray/Scrooge/Code/experiments/experiment_json/scale_clients.json client_scaling_experiment server_# where # = the number of the machine
