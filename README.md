# BFT-RSM

![](https://github.com/gupta-suyash/BFT-RSM/workflows/Demo/badge.svg?event=push)


How to make code:
1. Run setup scripts: sudo ./setup.sh
2. protoc -I=./system/protobuf --cpp_out=./system ./system/protobuf/crosschainmessage.proto
3. make

Note: You only need to run steps 1 and 2 once per new machine


For @murray22: 

To run the program (assuming only 2 clusters as of right now): /proj/ove-PG0/therealmurray/BFT-RSM/Code/experiments/experiment_scripts/run_experiments.py /proj/ove-PG0/therealmurray/BFT-RSM/Code/experiments/experiment_json/experiments.json one_shot_one2one

Setup script: /proj/ove-PG0/therealmurray/BFT-RSM/Code/experiments/experiment_scripts/setup.py /proj/ove-PG0/therealmurray/BFT-RSM/Code/experiments/experiment_json/experiments.json

Run algorand: /proj/ove-PG0/therealmurray/BFT-RSM/Code/experiments/experiment_scripts/algorand/setup_algorand.py 1 2 4 test1

Kill all python processes: killall -9 ssh; pkill python
