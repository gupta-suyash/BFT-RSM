#!/bin/bash

#make clean
make proto
make scrooge -j
./experiments/experiment_scripts/run_experiments.py /proj/ove-PG0/murray/BFT-RSM/Code/experiments/experiment_json/experiments.json $1 > myout.txt
