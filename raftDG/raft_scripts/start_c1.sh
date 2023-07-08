#! /bin/bash

# Used to start first Raft cluster on 4 nodes from the 9th remote node.

private_ips=(
  "10.10.1.1" 
  "10.10.1.2" 
  "10.10.1.3"
  "10.10.1.4" 
)
raft_ports=(
  "11379" 
  "12379" 
  "13379"
  "14379" 
)
kv_ports=(
  "11380" 
  "12380" 
  "13380"
  "14380" 
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
}

commands=(
  # kill user process running on $RAFT_PORT (potentially old raft instance)
#   "sudo fuser -n tcp -k \$RAFT_PORT"

  "cd /proj/ove-PG0/ethanxu/BFT-RSM/raftDG/etcd-main/contrib/raftexample"

  # delete snapshots so that cluster starts freshly from term 0
#   "./clear_snap.sh"

  "go build -o raftexample"

  "./raftexample --id \$CLUSTER_ID --cluster http://10.10.1.1:11379,http://10.10.1.2:12379,http://10.10.1.3:13379,http://10.10.1.4:14379 --port \$KV_PORT \> tmpout.txt"
)

runcmd "${commands[@]}"