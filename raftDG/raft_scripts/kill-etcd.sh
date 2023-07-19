#! /bin/bash

HOSTS=("10.10.1.1" "10.10.1.2" "10.10.1.3" "10.10.1.4" "10.10.1.5" "10.10.1.6" "10.10.1.7" "10.10.1.8")

for i in ${!HOSTS[@]}; do

    ssh -o StrictHostKeyChecking=no ${HOSTS[$i]} "sudo fuser -n tcp -k 2379 2380; sudo rm -rf \${HOME}/data.etcd; exit" &

done

wait