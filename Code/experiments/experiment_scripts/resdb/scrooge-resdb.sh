#!/bin/bash

## How to call this scripts: ./scrooge-resdb.sh <dir for resdb>

echo "Starting ResDB RSM1"
cd $1/deploy/config
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

################ TODO FIGURE OUT HOW TO ONLY START ONE

echo "Starting ResDB RSM2"
cd config
cp rsm2.conf kv_performance_server.conf
cd ..

echo "Killing old VMs"
./script/kill_server.sh config/kv_performance_server.conf
./script/kill_server.sh config/kv_performance_server.conf

echo "Compiling RSM2"
./script/deploy.sh ./config/kv_performance_server.conf

echo "Running Bazel Client"
bazel run //example:kv_server_tools -- $PWD/config_out/client.config set test 1234

stress -c 3 -m 3
