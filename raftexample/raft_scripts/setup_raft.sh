#! /bin/sh

# Update system, install go and etcd, and build "raftexample" on new remote node intances.
# Takes roughly 5 minutes to complete.

set -e # Exit immediately if any command fails

eval "$(ssh-agent -s)"
ssh-add ~/.ssh/cloudlab_rsa
ssh-add ~/.ssh/id_ed25519
export SSH_AUTH_SOCK

username="ethanxu"
hosts=(
    "hp066.utah.cloudlab.us" 
    "hp055.utah.cloudlab.us" 
    "hp062.utah.cloudlab.us"
    "hp051.utah.cloudlab.us" 
    "hp050.utah.cloudlab.us" 
    "hp056.utah.cloudlab.us"
    "hp064.utah.cloudlab.us" 
    "hp065.utah.cloudlab.us" 
)
deploy_hostlist=("${hosts[@]}")
echo "setting up: ${deploy_hostlist[@]}"

# Commands functions
function setup_raft() {
    
    # Completion status array to track progress of each remote node
    completion_status=()

    for i in "${!deploy_hostlist[@]}";
    do 
      hostname="${deploy_hostlist[$i]}"
      (
        echo "Setting up Raft on host with username: ${username}, hostname: ${hostname}"
        scp $(pwd)/setup.sh $(pwd)/clear_snap.sh ${username}@${hostname}:\$HOME
        ssh -A -v -n -o BatchMode=yes -o StrictHostKeyChecking=no ${username}@${hostname} "$1; $2"
      
        # Store completion status into array
        completion_status[$i]=$?
      ) &
    done

    # Wait for all background processes to finish
    wait 

    # Check the completion status of each remote node
    for i in "${!completion_status[@]}"; do
        if [[ ${completion_status[$i]} -eq 0 ]]; then
            echo "Remote node ${deploy_hostlist[$i]} has finished executing setup.sh"
        else
            echo "Remote node ${deploy_hostlist[$i]} encountered an error while executing setup.sh"
        fi
    done

}

commands=(
    "chmod +x \$HOME/setup.sh \$HOME/clear_snap.sh"
    "\$(\$HOME/setup.sh)"
)

setup_raft "${commands[@]}"
