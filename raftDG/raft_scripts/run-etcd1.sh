#! /bin/bash

TOKEN=token-333
CLUSTER_STATE=new

NAMES=("machine-1" "machine-2" "machine-3" "machine-4")
HOSTS=("10.10.1.1" "10.10.1.2" "10.10.1.3" "10.10.1.4")
CLUSTER=${NAMES[0]}=http://${HOSTS[0]}:2380,${NAMES[1]}=http://${HOSTS[1]}:2380,${NAMES[2]}=http://${HOSTS[2]}:2380,${NAMES[3]}=http://${HOSTS[3]}:2380

function run_etcd() {
    # for i in ${!HOSTS[@]}; do

    #     ssh -o StrictHostKeyChecking=no ${HOSTS[$i]} "sudo fuser -n tcp -k 2379 2380; sudo rm -rf \${HOME}/data.etcd; exit" &

    # done
    # wait

    for i in ${!HOSTS[@]}; do
        this_name=${NAMES[$i]}
        this_ip=${HOSTS[$i]}

        (ssh -o StrictHostKeyChecking=no ${HOSTS[$i]} "export THIS_NAME=${this_name}; export THIS_IP=${this_ip}; export TOKEN=${TOKEN}; export CLUSTER_STATE=${CLUSTER_STATE}; export CLUSTER=${CLUSTER}; export PATH=\$PATH:/proj/ove-PG0/ethanxu/BFT-RSM/raftDG/bin:/proj/ove-PG0/ethanxu/BFT-RSM/raftDG/etcd-main/bin; $1; $2") &
    done
    wait
}


run_commands=(
    "echo \$THIS_NAME, \$THIS_IP, \$TOKEN, \$CLUSTER_STATE, \$CLUSTER"
    "etcd --data-dir=data.etcd --name \${THIS_NAME} --initial-advertise-peer-urls http://\${THIS_IP}:2380 --listen-peer-urls http://\${THIS_IP}:2380 --advertise-client-urls http://\${THIS_IP}:2379 --listen-client-urls http://\${THIS_IP}:2379 --initial-cluster \${CLUSTER} --initial-cluster-state \${CLUSTER_STATE} --initial-cluster-token \${TOKEN}"
)

run_etcd "${run_commands[@]}"

