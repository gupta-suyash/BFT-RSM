#!/bin/bash
## Created By: Suyash Gupta - 08/23/2023
## THIS IS A TESTING SCRIPT DO NOT USE THIS FOR THE FINAL RESULTS
## This script helps to create the files that specify URLs for RSM1 and RSM2. Additionally, it calls the script that helps to create "config.h". We need to specify the URLs and stakes for each node in both the RSMs. This script takes in argument the size of both the RSMs and other necessary parameters.

if [ -z ${TMUX+x} ]; then
	echo "Run script in tmux to guarantee progress"
	echo "exiting..."
	exit 1
fi

echo -n "Enter the name of the experiment being run: "

read experiment_name

echo "Running Experiment: ${experiment_name}"

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
algorand_app_dir="${workdir}/BFT-RSM/Code/experiments/applications/algorand/"
algorand_scripts_dir="${workdir}/BFT-RSM/Code/experiments/experiment_scripts/algorand/"
resdb_dir="${workdir}/BFT-RSM/Code/experiments/experiment_scripts/resdb/"
raft_dir="${workdir}/BFT-RSM/Code/experiments/experiment_scripts/raft/"
use_debug_logs_bool="false"
max_nng_blocking_time=500ms
message_buffer_size=256

# Set rarely changing experiment application parameters
starting_algos = 10000000000000000

#
# EXPERIMENT LIST
#
# State all experiments you want to run below.
# State the stake of RSM1 and RSM2.
# Uncomment experiment you want to run.

# If you want to run all the three protocols, set them all to true. Otherwise, set only one of them to true.
scrooge="true"
all_to_all="false"
one_to_one="false"

#If this experiment is for File_RSM (not algo or resdb)
file_rsm="true"
# If this experiment uses external applications, set the following values
# Valid inputs: "algo", "resdb", "raft"
# e.x. if algorand is the sending RSM then send_rsm="algo", if resdb is
# receiving RSM, then receive_rsm="resdb"
send_rsm="algo"
receive_rsm="algo"

if [ "$file_rsm" = "false" ]; then
	echo "WARNING: FILE RSM NOT BEING USED"
fi

### DUMMY Exp: Equal stake RSMs of size 4; message size 100.
rsm1_size=(4 13)
rsm2_size=(4 13)
rsm1_fail=(0 0)
rsm2_fail=(0 0)
# RSM1_Stake=(1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1)
# RSM2_Stake=(1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1)

# Build the network from the description
num_nodes_rsm_1=0
num_nodes_rsm_2=0
for v in ${rsm1_size[@]}; do
    if (( $v > $num_nodes_rsm_1 )); then num_nodes_rsm_1=$v; fi; 
done
for v in ${rsm2_size[@]}; do
    if (( $v > $num_nodes_rsm_2 )); then num_nodes_rsm_2=$v; fi; 
done

GP_NAME="scrooge-exp"
ZONE="us-west1-b"

function exit_handler() {
        echo "** Trapped CTRL-C, deleting experiment"
		yes | gcloud compute instance-groups managed delete $GP_NAME --zone $ZONE
		exit 1
}

trap exit_handler INT
yes | gcloud beta compute instance-groups managed create "${GP_NAME}" --project=scrooge-398722 --base-instance-name="${GP_NAME}" --size="$((num_nodes_rsm_1+num_nodes_rsm_2))" --template=projects/scrooge-398722/global/instanceTemplates/scrooge-worker-template --zone=us-west1-b --list-managed-instances-results=PAGELESS --stateful-internal-ip=interface-name=nic0,auto-delete=never --no-force-update-on-repair > /dev/null 2>&1

rm /tmp/all_ips.txt
num_ips_read=0
while ((num_ips_read < $((num_nodes_rsm_1+num_nodes_rsm_2)))); do
	gcloud compute instances list --filter="name~^${GP_NAME}" --format='value(networkInterfaces[0].networkIP)' > /tmp/all_ips.txt
	output=$(cat /tmp/all_ips.txt)
	ar=($output)
	num_ips_read="${#ar[@]}"
done

RSM1=(${ar[@]::${num_nodes_rsm_1}})
RSM2=(${ar[@]:${num_nodes_rsm_2}})


#
# CREATING Config.h and network files.
#

for r1_size in "${rsm1_size[@]}"; do # Looping over all the network sizes
	# Setup all necessary external applications
	# Sending RSM
	if [ "$send_rsm" = "algo" ]; then
		start_algorand(${RSM1}, $r1_size)
	elif [ "$send_rsm" = "resdb" ]; then
		echo "ResDB RSM is being used for sending."
		start_resdb(${RSM1}, $r1_size)
	elif [ "$send_rsm" = "raft" ]; then
		echo "Raft RSM is being used for sending."
		start_raft(${RSM1}, $r1_size)
	elif [ "$send_rsm" = "file" ]; then
		echo "File RSM is being used for sending. No extra setup necessary."
	else; then
		echo "INVALID RECEIVING RSM."
	fi

	# Receiving RSM
	if [ "$receive_rsm" = "algo" ]; then
		echo "Algo RSM is being used for receiving."
		start_algorand(${RSM2}, $r1_size)
	elif [ "$receive_rsm" = "resdb" ]; then
		echo "ResDB RSM is being used for receiving."
		start_resdb(${RSM2}, $r1_size)
	elif [ "$receive_rsm" = "raft" ]; then
		echo "Raft RSM is being used for receiving."
		start_raft(${RSM2}, $r1_size)
	elif [ "$receive_rsm" = "file" ]; then
		echo "File RSM is being used for receiving. No extra setup necessary."
	else; then
		echo "INVALID RECEIVING RSM."
	fi
done

echo "taking down experiment"
yes | gcloud compute instance-groups managed delete $GP_NAME --zone $ZONE

############# DID YOU DELETE THE MACHINES?????????????????


start_algorand(RSM, rsm_size) {
	echo "Algo RSM is being used!"
	genesis_json = ${algorand_scripts_dir}/genesis.json;
	config_json = ${algorand_scripts_dir}/config.json;
	per_node_algos = ${starting_algos}/${rsm_size};
	mkdir ./genesis_creation/
	mkdir ./addresses/
	#Relay node - TODO MAKE SURE THIS IS SOMETHING SPECIAL
	ssh -oStrictHostKeyChecking=no -t "${RSM[0]}" '${algorand_script_dir}/setup_algorand_nodes.py ${algorand_app_dir} ${algorand_script_dir} ${per_node_algos} 0';
	#Participation nodes
	parallel -v --jobs=0 ssh -oStrictHostKeyChecking=no -t {1} '${algorand_script_dir}/setup_algorand_nodes.py ${genesis_json} ${config_json} ${algorand_app_dir} ${algorand_script_dir} ${per_node_algos}' ::: "${RSM[@]:1:$rsm_size}";
	# Get genesis + address files via scp TODO
	parallel -v --jobs=0 scp -oStrictHostKeyChecking=no -i "${key_file}" ${network_dir}{1} ${username}@{2}:"${exec_dir}" ::: ${algorand_json_dir}/*.json  ${algorand_json_dir}/*.json ::: "${RSM1[@]:0:$r1_size}"
	# Combine genesis pieces into one file
	while ((count < r1_size)); do
		 echo $(jq 'input as { $dummy } | .alloc |= . + [{ $dummy }]' genesis.json ${RSM1[$count]}.json) > genesis.json
		count=$((count + 1))
	done;
	# Redistribute all genesis files
	parallel -v --jobs=0 scp -oStrictHostKeyChecking=no -i "${key_file}" ${network_dir}{1} ${username}@{2}:"${exec_dir}" ::: genesis.json config.json ::: "${RSM[@]:0:$rsm_size}";
	# Determine address mappings
	# Finish wallet setup + run Algorand
	parallel -v --jobs=0 ssh -oStrictHostKeyChecking=no -t {1} '${algorand_script_dir}/setup_algorand_nodes.py ${genesis_json} ${config_json} ${algorand_app_dir} ${algorand_script_dir} ${per_node_algos}' ::: "${RSM[@]:1:$rsm_size}";
}

start_resdb() {
	echo "ResDB RSM is being used!"
	
}

start_raft() {
	echo "Raft RSM is being used!"

}


