#!/bin/bash

cd /proj/ove-PG0/ethanxu
echo "Starting ResDB RSM1"
cd resilientdb/deploy/config
echo $PWD
cp rsm1.conf kv_performance_server.conf
cd ..

echo "Killing old VMs"
./script/kill_server.sh config/kv_performance_server.conf
./script/kill_server.sh config/kv_performance_server.conf
