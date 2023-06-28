#! /bin/bash

# Used to all both Raft clusters of 4 nodes from the 9th remote node.

private_ips=(
  "10.10.1.1" 
  "10.10.1.2" 
  "10.10.1.3"
  "10.10.1.4" 
  "10.10.1.5" 
  "10.10.1.6" 
  "10.10.1.7"
  "10.10.1.8" 
)
raft_ports=(
  "11379" 
  "12379" 
  "13379"
  "14379" 
  "15379" 
  "16379" 
  "17379"
  "18379"
)
kv_ports=(
  "11380" 
  "12380" 
  "13380"
  "14380" 
  "15380" 
  "16380" 
  "17380"
  "18380" 
)

# Commands functions
function runcmd() {
    for i in "${!private_ips[@]}";
    do 
      cluster_id=$((i+1))
      private_ip="${private_ips[$i]}"
      raft_port="${raft_ports[$i]}"
      kv_port="${kv_ports[$i]}"
      (
        echo "Running Raft on host with username: ${username}, cluster id: ${cluster_id}, raft port: ${raft_port}, kv port: ${kv_port}"
        ssh -A -v -o StrictHostKeyChecking=no ${private_ip} "export CLUSTER_ID=${cluster_id}; export RAFT_PORT=${raft_port}; export KV_PORT=${kv_port}; $1; $2; $3"
      ) &
    done

    wait
}

commands=(
  # kill user process running on $RAFT_PORT (potentially old raft instance)
  "sudo fuser -n tcp -k \$RAFT_PORT"

  "cd /proj/ove-PG0/ethanxu/BFT-RSM/raftDG/etcd-main/contrib/raftexample"

  # delete snapshots so that cluster starts freshly from term 0
  "./clear_snap.sh"
)

runcmd "${commands[@]}"