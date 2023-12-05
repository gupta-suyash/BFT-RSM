#!/bin/bash
## Created By: Suyash Gupta - 08/23/2023
##
## This script helps to create the files that specify URLs for RSM1 and RSM2. Additionally, it calls the script that helps to create "config.h". We need to specify the URLs and stakes for each node in both the RSMs. This script takes in argument the size of both the RSMs and other necessary parameters.

echo "DONT FORGET YOU HARDCODED THE NETWORK SIZE IN HERE!"

if [ -z ${TMUX+x} ]; then
	echo "Run script in tmux to guarantee progress"
	echo "exiting..."
	exit 1
fi

# Valid inputs: "algo", "resdb", "raft"
# e.x. if algorand is the sending RSM then send_rsm="algo", if resdb is
# receiving RSM, then receive_rsm="resdb"

#If this experiment is for File_RSM (not algo or resdb)


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

#
# EXPERIMENT LIST
#
# State all experiments you want to run below.
# State the stake of RSM1 and RSM2.
# Uncomment experiment you want to run.

# If you want to run all the three protocols, set them all to true. Otherwise, set only one of them to true.


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

echo "Stopping Algorand Nodes!"
r1_size=4
	# Next, we create the configuration file "network1urls.txt" through echoing and redirection.

	############# Setup all necessary external applications #############
	# Setup all necessary external applications
	function start_algorand() {
		echo "######################################################Algorand RSM is being used!"
		# Take in arguments
		local client_ip=$1
		local size=$2
		local RSM=("${!3}")
		echo "${RSM[@]}"
        
        # Step 1: Copy shutdown script onto each machine
		scp -o StrictHostKeyChecking=no -i "${key_file}" ${algorand_scripts_dir}/scripts/shutdown.sh ${username}@${client_ip}:${workdir}
		parallel -v --jobs=0 scp -o StrictHostKeyChecking=no -i "${key_file}" ${algorand_scripts_dir}/scripts/shutdown.sh ${username}@{1}:${workdir} ::: "${RSM[@]:0:$((size))}";

		# Step 2: Execute shutdown script
		ssh -o StrictHostKeyChecking=no -t "${client_ip}" '/home/scrooge/shutdown.sh '"${algorand_scripts_dir}"''
		parallel -v --jobs=0 ssh -o StrictHostKeyChecking=no -t {1} '/home/scrooge/shutdown.sh '"${algorand_scripts_dir}"'' ::: "${RSM[@]:0:$((size))}";
	}
	
	start_algorand "${CLIENT[0]}" "$r1_size" "RSM1[@]"
	start_algorand "${CLIENT[1]}" "$r1_size" "RSM2[@]"
