#!/bin/bash

echo "Starting ResDB RSM1"
#cd /home/scrooge/BFT-RSM/Code/experiments/applications/resdb/deploy/config
echo $PWD

echo "Killing old VMs"
/home/scrooge/BFT-RSM/Code/experiments/applications/resdb/deploy/script/kill_server.sh  /home/scrooge/BFT-RSM/Code/experiments/applications/resdb/deploy/config/kv_performance_server.conf
/home/scrooge/BFT-RSM/Code/experiments/applications/resdb/deploy/script/kill_server.sh  /home/scrooge/BFT-RSM/Code/experiments/applications/resdb/deploy/config/kv_performance_server.conf
