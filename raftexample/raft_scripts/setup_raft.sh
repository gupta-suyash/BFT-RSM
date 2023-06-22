#! /bin/sh

# update system, install go and etcd, and build "raftexample" on new remote node intances

set -e # Exit immediately if any command fails

eval "$(ssh-agent -s)"
ssh-add ~/.ssh/cloudlab_rsa
ssh-add ~/.ssh/id_ed25519
export SSH_AUTH_SOCK

username="ethanxu"
hosts=("hp055.utah.cloudlab.us" "hp062.utah.cloudlab.us")
deploy_hostlist=("${hosts[@]}")
echo "setting up: ${deploy_hostlist[@]}"

# Commands functions
function runcmd() {
    for i in "${!deploy_hostlist[@]}";
    do 
      hostname="${deploy_hostlist[$i]}"
      (
        echo "Setting up Raft on host with username: ${username}, hostname: ${hostname}"
        scp $(pwd)/setup.sh $(pwd)/clear_snap.sh ${username}@${hostname}:\$HOME
        ssh -A -v -n -o BatchMode=yes -o StrictHostKeyChecking=no ${username}@${hostname} "$1; $2"
      ) &
    done
}

commands=(
    "chmod +x \$HOME/setup.sh \$HOME/clear_snap.sh"
    "\$(\$HOME/setup.sh)"
)

runcmd "${commands[@]}"
