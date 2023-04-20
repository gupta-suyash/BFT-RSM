#!/bin/bash

# $1 - name of the result folder you want
# $2 - cluster number, 0 or 1

echo "Starting ResDB RSM1"

cd resilientdb/deploy/config
echo $PWD
cp rsm${2}.conf kv_performance_server.conf
cd ..

echo "Killing old VMs"
./script/get_results.sh config/kv_performance_server.conf $2

cd exp-results
mkdir res_${1}_C${2}
cd res_${1}_C${2}
mv ../../kv_server_performance* .
cd ../../

echo $PWD


