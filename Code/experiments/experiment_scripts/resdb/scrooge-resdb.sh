#!/bin/bash

cd /proj/ove-PG0/murray/
echo "Starting ResDB RSM1"
cd resilientdb/deploy/config
echo $PWD
cp rsm1.conf kv_performance_server.conf
cd ..

echo "Killing old VMs"
./script/kill_server.sh config/kv_performance_server.conf
./script/kill_server.sh config/kv_performance_server.conf

echo "Compiling RSM1"
./script/deploy.sh ./config/kv_performance_server.conf

echo "Running Bazel Client"
bazel run //example:kv_server_tools -- $PWD/config_out/client.config set test 1234

#echo "Starting ResDB RSM2"
#cd config
#cp rsm2.conf kv_performance_server.conf
#cd ..

#echo "Killing old VMs"
#./script/kill_server.sh config/kv_performance_server.conf
#./script/kill_server.sh config/kv_performance_server.conf

#echo "Compiling RSM2"
#./script/deploy.sh ./config/kv_performance_server.conf

#echo "Running Bazel Client"
#bazel run //example:kv_server_tools -- $PWD/config_out/client.config set test 1234

#stress -c 3 -m 3


cd /proj/ove-PG0/murray/BFT-RSM/Code
echo $PWD

echo "Starting Scrooge"
#cd BFT-RSM/Code
#echo $PWD

#make clean
#make proto
#make scrooge -j

./experiments/experiment_scripts/run_experiments.py /proj/ove-PG0/murray/BFT-RSM/Code/experiments/experiment_json/experiments.json increase_packet_size_nb_one > myout.txt
