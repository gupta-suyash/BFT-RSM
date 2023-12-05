#!/bin/bash
## Created By: Suyash Gupta - 08/23/2023
##
## This script helps to create the files that specify URLs for RSM1 and RSM2. Additionally, it calls the script that helps to create "config.h". We need to specify the URLs and stakes for each node in both the RSMs. This script takes in argument the size of both the RSMs and other necessary parameters.

if [ -z ${TMUX+x} ]; then
	echo "Run script in tmux to guarantee progress"
	echo "exiting..."
	exit 1
fi


# If this experiment uses external applications, set the following values
# Valid inputs: "algo", "resdb", "raft"
# e.x. if algorand is the sending RSM then send_rsm="algo", if resdb is

# Name of profile we are running out of
key_file="$HOME/.ssh/id_ed25519" # TODO: Replace with your ssh key
username="scrooge"               # TODO: Replace with your username

# Working Directory
workdir="/home/scrooge"

# Set rarely changing Scrooge parameters.
warmup_time=20s
total_time=120s
num_packets=10000
exec_dir="$HOME/"
network_dir="${workdir}/BFT-RSM/Code/configuration/"
log_dir="${workdir}/BFT-RSM/Code/experiments/results/"
json_dir="${workdir}/BFT-RSM/Code/experiments/experiment_json/"
algorand_app_dir="${workdir}/BFT-RSM/Code/experiments/applications/go-algorand/"
resdb_app_dir="${workdir}/BFT-RSM/Code/experiments/applications/resdb/"
raft_app_dir="${workdir}/BFT-RSM/Code/experiments/applications/raft-application/"
algorand_scripts_dir="${workdir}/BFT-RSM/Code/experiments/experiment_scripts/algorand/"
resdb_scripts_dir="${workdir}/BFT-RSM/Code/experiments/experiment_scripts/resdb/"
raft_scripts_dir="${workdir}/BFT-RSM/Code/experiments/experiment_scripts/raft/"
use_debug_logs_bool="false"
max_nng_blocking_time=500ms
message_buffer_size=256

# Set rarely changing experiment application parameters
starting_algos=10000000000000000

#
# EXPERIMENT LIST
#
# State all experiments you want to run below.
# State the stake of RSM1 and RSM2.
# Uncomment experiment you want to run.

# If you want to run all the three protocols, set them all to true. Otherwise, set only one of them to true.


### DUMMY Exp: Equal stake RSMs of size 4; message size 100.
rsm1_size=(4)
rsm2_size=(4)
rsm1_fail=(0)
rsm2_fail=(0)
RSM1_Stake=(1 1 1 1)
RSM2_Stake=(1 1 1 1)
klist_size=(64)
packet_size=(100)
batch_size=(200000)
batch_creation_time=(1ms)
pipeline_buffer_size=(8)

# Build the network from the description
num_nodes_rsm_1=0
num_nodes_rsm_2=0
client=2
for v in ${rsm1_size[@]}; do
    if (( $v > $num_nodes_rsm_1 )); then num_nodes_rsm_1=$v; fi; 
done
for v in ${rsm2_size[@]}; do
    if (( $v > $num_nodes_rsm_2 )); then num_nodes_rsm_2=$v; fi; 
done

echo "SET RSM SIZES"
echo "$num_nodes_rsm_1"
echo "$num_nodes_rsm_2"
RSM1=(10.128.6.20 10.128.6.21 10.128.6.22 10.128.6.23)
RSM2=(10.128.6.24 10.128.6.25 10.128.6.26 10.128.6.27)
CLIENT=(10.128.6.28 10.128.6.29)

count=0
while ((${count} < ${num_nodes_rsm_1})); do
	echo "RSM1: ${RSM1[$count]}"
	count=$((count + 1))
	if [ ${count} -eq "${num_nodes_rsm_1}" ]; then
		break
	fi
done
count=0
while ((${count} < ${num_nodes_rsm_2})); do
	echo "RSM2: ${RSM2[$count]}"
	count=$((count + 1))
	if [ ${count} -eq "${num_nodes_rsm_2}" ]; then
		break
	fi
done
count=0
while ((${count} < ${client})); do
	echo "Client: ${CLIENT[$count]}"
	count=$((count + 1))
	if [ ${count} -eq "${client}" ]; then
		break
	fi
done

echo "Starting Algorand Nodes!"
r1_size=4
function start_algorand() {
        echo "######################################################Algorand RSM is being used!"
        # Take in arguments
        local client_ip=$1
        local size=$2
        local RSM=("${!3}")
        echo "${RSM[@]}"

        genesis_json=${algorand_scripts_dir}/scripts/genesis.json;
        cat $genesis_json
        per_node_algos=$((starting_algos / size));
        echo $per_node_algos
        rm -rf ${algorand_scripts_dir}/genesis_creation
        rm -rf ${algorand_scripts_dir}/addresses
        mkdir ${algorand_scripts_dir}/genesis_creation/
        cp $genesis_json ${algorand_scripts_dir}/genesis_creation/
        mkdir ${algorand_scripts_dir}/addresses/
        
        #Participation nodes
        parallel -v --jobs=0 ssh -o StrictHostKeyChecking=no -t {1} 'rm '"${algorand_scripts_dir}"'/{1}_gen.json' ::: "${RSM[@]:0:$((size))}";
        parallel -v --jobs=0 ssh -o StrictHostKeyChecking=no -t {1} 'rm '"${algorand_scripts_dir}"'/{1}_addr.json ' ::: "${RSM[@]:0:$((size))}";


       
#Relay nodes (which also happens to be the client nodes)
        ssh -o StrictHostKeyChecking=no -t "${client_ip}" ''"${algorand_scripts_dir}"'/setup_algorand.py '"${algorand_app_dir}"' '"${algorand_scripts_dir}"' '"${algorand_scripts_dir}"'/scripts/relay_config.json '"${per_node_algos}"' '"${client_ip}"''
        echo "Sent Relay node information!"
        #Participation nodes
        parallel -v --jobs=0 ssh -o StrictHostKeyChecking=no -t {1} ''"${algorand_scripts_dir}"'/setup_algorand.py '"${algorand_app_dir}"' '"${algorand_scripts_dir}"' '"${algorand_scripts_dir}"'/scripts/node_config.json '"${per_node_algos}"' '"${client_ip}"'' ::: "${RSM[@]:0:$((size))}";

        ### Get genesis files ###
        parallel -v --jobs=0 scp -o StrictHostKeyChecking=no -i "${key_file}" ${username}@{1}:${algorand_scripts_dir}/{1}_gen.json ${algorand_scripts_dir}/genesis_creation/ ::: "${RSM[@]:0:$((size))}";
        # Combine genesis pieces into one file
        count=0
        while ((count < size)); do
            echo $(genesis=$(jq 'select(has("addr"))' ${algorand_scripts_dir}/genesis_creation/${RSM[$count]}_gen.json);jq --argjson genesis "$genesis" '.alloc += [ $genesis ]' ${algorand_scripts_dir}/genesis_creation/genesis.json) > ${algorand_scripts_dir}/genesis_creation/genesis.json
            count=$((count + 1))
        done
        # Copy final genesis files onto all machines
        scp -o StrictHostKeyChecking=no -i "${key_file}" ${algorand_scripts_dir}/genesis_creation/genesis.json ${username}@${client_ip}:${algorand_scripts_dir}/node/
        parallel -v --jobs=0 scp -o StrictHostKeyChecking=no -i "${key_file}" ${algorand_scripts_dir}/genesis_creation/genesis.json ${username}@{1}:${algorand_scripts_dir}/node/ ::: "${RSM[@]:0:$((size))}";

        ### Get address matchings ###
        parallel -v --jobs=0 scp -o StrictHostKeyChecking=no -i "${key_file}" ${username}@{1}:${algorand_scripts_dir}/{1}_addr.json ${algorand_scripts_dir}/addresses/ ::: "${RSM[@]:0:$((size))}";
        # Runn address swap for each machine file
        count=0
        while ((count < size)); do
            echo "COMMAND: ${algorand_scripts_dir}/addr_swap.py ${algorand_scripts_dir}/addresses ${RSM[$count]} ${RSM[$(((count-1) % size))]} ${RSM[$(((count+1) % size))]}"
            ${algorand_scripts_dir}/addr_swap.py ${algorand_scripts_dir}/addresses ${RSM[$count]} ${RSM[$(((count-1) % size))]} ${RSM[$(((count+1) % size))]}
            count=$((count + 1))
        done
        # Copy final wallet address files onto all machines
        parallel -v --jobs=0 scp -o StrictHostKeyChecking=no -i "${key_file}" ${algorand_scripts_dir}/addresses/{1}_node.json ${username}@{1}:${algorand_app_dir}/wallet_app/node.json ::: "${RSM[@]:0:$((size))}";

 }	
#start_algorand "${CLIENT[0]}" "$r1_size" "RSM1[@]"
start_algorand "${CLIENT[1]}" "$r1_size" "RSM2[@]"
