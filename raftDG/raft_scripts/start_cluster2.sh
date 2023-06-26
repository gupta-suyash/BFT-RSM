#! /bin/sh
eval "$(ssh-agent -s)"
ssh-add ~/.ssh/cloudlab_rsa
ssh-add ~/.ssh/id_ed25519
export SSH_AUTH_SOCK

username="ethanxu"
hosts=(
  "hp050.utah.cloudlab.us" 
  "hp056.utah.cloudlab.us"
  "hp064.utah.cloudlab.us" 
  "hp065.utah.cloudlab.us"
)
private_ips=( 
  "10.10.1.5" 
  "10.10.1.6"
  "10.10.1.7" 
  "10.10.1.8" 
)
raft_ports=(
  "15379" 
  "16379"
  "17379" 
  "18379" 
)
kv_ports=(
  "15380" 
  "16380"
  "17380" 
  "18380" 
)

deploy_hostlist=("${hosts[@]}")
echo "deploying to: ${deploy_hostlist[@]}"

# Commands functions
function runcmd() {
    for i in "${!deploy_hostlist[@]}";
    do 
      cluster_id=$((i+1))
      hostname="${deploy_hostlist[$i]}"
      raft_port="${raft_ports[$i]}"
      kv_port="${kv_ports[$i]}"
      (
        echo "Running Raft on host with username: ${username}, hostname: ${hostname}, cluster id: ${cluster_id}, raft port: ${raft_port}, kv port: ${kv_port}"
        ssh -A -v -n -o BatchMode=yes -o StrictHostKeyChecking=no ${username}@${hostname} "export CLUSTER_ID=${cluster_id}; export PRIVATE_IPS=${private_ips[@]}; export RAFT_PORT=${raft_port}; export KV_PORT=${kv_port}; $1; $2; $3; $4"
      ) &
    done
}

commands=(
  # kill user process running on $RAFT_PORT (potentially old raft instance)
  "sudo fuser -n tcp -k \$RAFT_PORT"

  # delete snapshots so that cluster starts freshly from term 0
  "./clear_snap.sh"

  "cd ~/go/src/go.etcd.io/etcd/contrib/raftexample"

  "./raftexample --id \$CLUSTER_ID --cluster http://10.10.1.5:15379,http://10.10.1.6:16379,http://10.10.1.7:17379,http://10.10.1.8:18379 --port \$KV_PORT"
)

runcmd "${commands[@]}"