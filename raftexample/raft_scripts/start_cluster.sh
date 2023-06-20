#! /bin/sh
eval "$(ssh-agent -s)"
ssh-add ~/.ssh/cloudlab_rsa
ssh-add ~/.ssh/id_ed25519
export SSH_AUTH_SOCK

username="ethanxu"
hosts=("hp078.utah.cloudlab.us" "hp075.utah.cloudlab.us" "hp073.utah.cloudlab.us")
private_ips=("10.10.1.1" "10.10.1.2" "10.10.1.3")
raft_ports=("12379" "22379" "32379")
kv_ports=("12380" "22380" "32380")
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
  "sudo fuser -n tcp -k \$RAFT_PORT"
  "./clear_snap.sh"
  "cd ~/go/src/go.etcd.io/etcd/contrib/raftexample"
  "./raftexample --id \$CLUSTER_ID --cluster http://10.10.1.1:12379,http://10.10.1.2:22379,http://10.10.1.3:32379 --port \$KV_PORT"
)

runcmd "${commands[@]}"