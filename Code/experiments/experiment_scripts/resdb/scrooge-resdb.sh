#!/bin/bash

## How to call this scripts: ./scrooge-resdb.sh <dir for resdb> <cluster number> <dir for resdb scripts>

echo "PARAMETERS PRINTING NOW!!!!!!!!!!"
echo $1
echo $2
echo $3
echo "DONE PRINTING PARAMETERS!!!!!!!!!"

echo "Starting ResDB RSM1"
cd $1/deploy/config
echo $PWD
#cp rsm$2.conf kv_performance_server.conf
cd ..

echo "Killing old VMs"
./script/kill_server.sh config/kv_performance_server.conf
./script/kill_server.sh config/kv_performance_server.conf

echo "Compiling RSM"
./script/deploy.sh ./config/kv_performance_server.conf

echo "Running Bazel Client"
bazel run //example:kv_server_tools -- $PWD/config_out/client.config set test 1234
#stress -c 3 -m 3


