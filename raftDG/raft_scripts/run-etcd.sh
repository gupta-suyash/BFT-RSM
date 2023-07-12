#! /bin/bash

TOKEN=token-01
CLUSTER_STATE=new

# NAMES=("machine-1" "machine-2" "machine-3")
# HOSTS=("10.10.1.1" "10.10.1.2" "10.10.1.3")
# CLUSTER=${NAMES[0]}=http://${HOSTS[0]}:2380,${NAMES[1]}=http://${HOSTS[1]}:2380,${NAMES[2]}=http://${HOSTS[2]}:2380

NAMES=("machine-3")
HOSTS=("10.10.1.3")
CLUSTER=${NAMES[0]}=http://${HOSTS[0]}:2380

function run_etcd() {
    for i in ${!HOSTS[@]}; do

        ssh -o StrictHostKeyChecking=no ${HOSTS[$i]} "$(declare -f kill_etcd); kill_etcd; exit" &
        wait
        echo "finished clearing snapshots, sleeping for 5 seconds"
        sleep 5
        ssh -o StrictHostKeyChecking=no ${HOSTS[$i]} "$1; $2; $3; $4l $5" &
        wait
        
    done
    wait
}

function kill_etcd() {
    sudo fuser -n tcp -k 2379 2380
    cd /proj/ove-PG0/ethanxu/BFT-RSM/raftDG/raft_scripts/
    rm -rf data*.etcd
}

run_commands=(
    "source \$HOME/.profile"
    "THIS_NAME=${NAMES[2]}"
    "THIS_IP=${HOSTS[2]}"
    'echo "THIS_NAME: \$THIS_NAME, THIS_IP: \$THIS_IP"'
    "etcd --data-dir=data.etcd --name \${THIS_NAME} \\
        --initial-advertise-peer-urls http://\${THIS_IP}:2380 --listen-peer-urls http://\${THIS_IP}:2380 \\
        --advertise-client-urls http://\${THIS_IP}:2379 --listen-client-urls http://\${THIS_IP}:2379 \\
        --initial-cluster \${CLUSTER} \\
        --initial-cluster-state \${CLUSTER_STATE} --initial-cluster-token \${TOKEN}"
)

run_etcd ${run_commands[@]}

