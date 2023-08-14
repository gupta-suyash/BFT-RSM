#! /bin/bash

# Assume we are running etcd clusters in present working directory (where BFT-RSM has been installed)
current_path=$(pwd)
BFTRSM_path="${current_path}/BFT-RSM"
etcd_path="${BFTRSM_path}/raftDG/etcd-main"
etcd_bin_path="${etcd_path}/bin"
benchmark_bin_path="${BFTRSM_path}/raftDG/bin"


ScroogeCode_path="/proj/ove-PG0/ethanxu/BFT-RSM/Code"

TOKEN=token-77
CLUSTER_STATE=new

NAMES1=("machine-1" "machine-2" "machine-3" "machine-4" "machine-5" "machine-6" "machine-7")
HOSTS1=("10.10.1.1" "10.10.1.2" "10.10.1.3" "10.10.1.4" "10.10.1.5" "10.10.1.6" "10.10.1.7")
CLUSTER1=${NAMES1[0]}=http://${HOSTS1[0]}:2380,${NAMES1[1]}=http://${HOSTS1[1]}:2380,${NAMES1[2]}=http://${HOSTS1[2]}:2380,${NAMES1[3]}=http://${HOSTS1[3]}:2380,${NAMES1[4]}=http://${HOSTS1[4]}:2380,${NAMES1[5]}=http://${HOSTS1[5]}:2380,${NAMES1[6]}=http://${HOSTS1[6]}:2380

NAMES2=("machine-1" "machine-2" "machine-3" "machine-4" "machine-5" "machine-6" "machine-7")
HOSTS2=("10.10.1.8" "10.10.1.9" "10.10.1.10" "10.10.1.11" "10.10.1.12" "10.10.1.13" "10.10.1.14")
CLUSTER2=${NAMES2[0]}=http://${HOSTS2[0]}:2380,${NAMES2[1]}=http://${HOSTS2[1]}:2380,${NAMES2[2]}=http://${HOSTS2[2]}:2380,${NAMES2[3]}=http://${HOSTS2[3]}:2380,${NAMES2[4]}=http://${HOSTS2[4]}:2380,${NAMES2[5]}=http://${HOSTS2[5]}:2380,${NAMES2[6]}=http://${HOSTS2[6]}:2380

function kill_etcd() {
    echo "Killing old etcd..."
    for i in ${!HOSTS1[@]}; do
        # ssh -o StrictHostKeyChecking=no ${HOSTS1[$i]} "sudo fuser -n tcp -k 2379 2380; sudo rm -rf ${current_path}/data.etcd; exit" &
        ssh -o StrictHostKeyChecking=no ${HOSTS1[$i]} "killall -9 benchmark; sudo fuser -n tcp -k 2379 2380; sudo rm -rf \$HOME/data.etcd; exit" &
    done

    for j in ${!HOSTS2[@]}; do
        # ssh -o StrictHostKeyChecking=no ${HOSTS2[$j]} "sudo fuser -n tcp -k 2379 2380; sudo rm -rf ${current_path}/data.etcd; exit" &
        ssh -o StrictHostKeyChecking=no ${HOSTS2[$j]} "killall -9 benchmark; sudo fuser -n tcp -k 2379 2380; sudo rm -rf \$HOME/data.etcd; exit" &
    done
    wait

    sleep 5
    echo "Finished killing old etcd."
}


function run_etcd() {

    # Run the first etcd cluster
    for i in ${!HOSTS1[@]}; do
        this_name=${NAMES1[$i]}
        this_ip=${HOSTS1[$i]}

        (ssh -o StrictHostKeyChecking=no ${HOSTS1[$i]} "export THIS_NAME=${this_name}; export THIS_IP=${this_ip}; export TOKEN=${TOKEN}; export CLUSTER_STATE=${CLUSTER_STATE}; export CLUSTER=${CLUSTER1}; export PATH=\$PATH:${benchmark_bin_path}:${etcd_bin_path}; $1; $2; $3") &
    done

    # Run the second etcd cluster
    for j in ${!HOSTS2[@]}; do
        this_name=${NAMES2[$j]}
        this_ip=${HOSTS2[$j]}

        (ssh -o StrictHostKeyChecking=no ${HOSTS2[$j]} "export THIS_NAME=${this_name}; export THIS_IP=${this_ip}; export TOKEN=${TOKEN}; export CLUSTER_STATE=${CLUSTER_STATE}; export CLUSTER=${CLUSTER2}; export PATH=\$PATH:${benchmark_bin_path}:${etcd_bin_path}; $1; $2; $3") &
    done
}

function run_scrooge() {
    sleep 15
    echo "Starting Scrooge..."
    cd ${ScroogeCode_path}
    ${ScroogeCode_path}/experiments/experiment_scripts/run_experiments.py ${ScroogeCode_path}/experiments/experiment_json/experiments.json increase_packet_size_nb_one
}

function run_benchmark() {
    sleep 45
    echo "Running benchmark..."

    export PATH=$PATH:${benchmark_bin_path}:${etcd_bin_path}

    HOST_1="10.10.1.1:2379"
    HOST_2="10.10.1.2:2379"
    HOST_3="10.10.1.3:2379"
    HOST_4="10.10.1.4:2379"
    HOST_5="10.10.1.5:2379"
    HOST_6="10.10.1.6:2379"
    HOST_7="10.10.1.7:2379"

    HOST_8="10.10.1.8:2379"
    HOST_9="10.10.1.9:2379"
    HOST_10="10.10.1.10:2379"
    HOST_11="10.10.1.11:2379"
    HOST_12="10.10.1.12:2379"
    HOST_13="10.10.1.13:2379"
    HOST_14="10.10.1.14:2379"

    # benchmark --endpoints=${HOST_1},${HOST_2},${HOST_3},${HOST_4},${HOST_5},${HOST_6},${HOST_7} --conns=100 --clients=1000 put --key-size=8 --sequential-keys --total=1500000 --val-size=256
    # benchmark --endpoints=${HOST_8},${HOST_9},${HOST_10},${HOST_11},${HOST_12},${HOST_13},${HOST_14} --conns=100 --clients=1000 put --key-size=8 --sequential-keys --total=1500000 --val-size=256

    (benchmark --endpoints=${HOST_1},${HOST_2},${HOST_3},${HOST_4},${HOST_5},${HOST_6},${HOST_7} --conns=100 --clients=1000 put --key-size=8 --sequential-keys --total=1500000 --val-size=256 
    benchmark --endpoints=${HOST_1},${HOST_2},${HOST_3},${HOST_4},${HOST_5},${HOST_6},${HOST_7} --conns=100 --clients=1000 put --key-size=8 --sequential-keys --total=1500000 --val-size=256
    benchmark --endpoints=${HOST_1},${HOST_2},${HOST_3},${HOST_4},${HOST_5},${HOST_6},${HOST_7} --conns=100 --clients=1000 put --key-size=8 --sequential-keys --total=2000000 --val-size=256) &

    (benchmark --endpoints=${HOST_8},${HOST_9},${HOST_10},${HOST_11},${HOST_12},${HOST_13},${HOST_14} --conns=100 --clients=1000 put --key-size=8 --sequential-keys --total=1500000 --val-size=256
    benchmark --endpoints=${HOST_8},${HOST_9},${HOST_10},${HOST_11},${HOST_12},${HOST_13},${HOST_14} --conns=100 --clients=1000 put --key-size=8 --sequential-keys --total=1500000 --val-size=256
    benchmark --endpoints=${HOST_8},${HOST_9},${HOST_10},${HOST_11},${HOST_12},${HOST_13},${HOST_14} --conns=100 --clients=1000 put --key-size=8 --sequential-keys --total=2000000 --val-size=256) &

    wait
}

run_commands=(
    # "cd ${current_path}"
    "cd \$HOME"

    "echo PWD: \$(pwd)  THIS_NAME:\${THIS_NAME} THIS_IP:\${THIS_IP} TOKEN:\${TOKEN} CLUSTER:\${CLUSTER}" #for testing
    "etcd --data-dir=data.etcd --name \${THIS_NAME} --initial-advertise-peer-urls http://\${THIS_IP}:2380 --listen-peer-urls http://\${THIS_IP}:2380 --advertise-client-urls http://\${THIS_IP}:2379 --listen-client-urls http://\${THIS_IP}:2379 --initial-cluster \${CLUSTER} --initial-cluster-state \${CLUSTER_STATE} --initial-cluster-token \${TOKEN}"
)

kill_etcd
run_etcd "${run_commands[@]}"
run_scrooge &
run_benchmark