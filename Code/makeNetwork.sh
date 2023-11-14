#!/bin/bash
## Created By: Suyash Gupta - 08/23/2023
##
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
resdb_app_dir="${workdir}/BFT-RSM/Code/experiments/applications/resdb/"
raft_app_dir="${workdir}/BFT-RSM/Code/experiments/applications/raftDG/"
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
scrooge="true"
all_to_all="false"
one_to_one="false"

#If this experiment is for File_RSM (not algo or resdb)
#file_rsm="true"
file_rsm="false"
# If this experiment uses external applications, set the following values
# Valid inputs: "algo", "resdb", "raft"
# e.x. if algorand is the sending RSM then send_rsm="algo", if resdb is
# receiving RSM, then receive_rsm="resdb"
send_rsm="resdb"
receive_rsm="resdb"

if [ "$file_rsm" = "false" ]; then
	echo "WARNING: FILE RSM NOT BEING USED"
fi

### DUMMY Exp: Equal stake RSMs of size 4; message size 100.
rsm1_size=(4)
rsm2_size=(4)
rsm1_fail=(0)
rsm2_fail=(0)
RSM1_Stake=(1 1 1 1)
RSM2_Stake=(1 1 1 1)
klist_size=(64)
packet_size=(100 1000000)
batch_size=(200000)
batch_creation_time=(1ms)
pipeline_buffer_size=(8)

### DUMMY Exp: Equal stake RSMs of size 4; message size 100.
# rsm1_size=(4 13 25 46)
# rsm2_size=(4 13 25 46)
# rsm1_fail=(0 0 0 0)
# rsm2_fail=(0 0 0 0)
# RSM1_Stake=(1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1)
# RSM2_Stake=(1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1)
# klist_size=(64)
# packet_size=(100 1000000)
# batch_size=(200000)
# batch_creation_time=(1ms)
# pipeline_buffer_size=(8)

### Exp: Equal stake RSMs of size 4; message size 100.
#rsm1_size=(4)
#rsm2_size=(4)
#rsm1_fail=(1)
#rsm2_fail=(1)
#RSM1_Stake=(1 1 1 1)
#RSM2_Stake=(1 1 1 1)
#packet_size=(100)

## Exp: Equal stake RSMs of size 4; message size 100, 1000
#rsm1_size=(4)
#rsm2_size=(4)
#rsm1_fail=(1)
#rsm2_fail=(1)
#RSM1_Stake=(1 1 1 1)
#RSM2_Stake=(1 1 1 1)
#klist_size=(64)
#packet_size=(100 1000 10000 50000 100000)
#batch_size=(26214)
#batch_creation_time=(1ms)
#pipeline_buffer_size=(8)

### Exp: Equal stake RSMs of size 7; message size 1000.
#rsm1_size=(7)
#rsm2_size=(7)
#rsm1_fail=(2)
#rsm2_fail=(2)
#RSM1_Stake=(1 1 1 1 1 1 1)
#RSM2_Stake=(1 1 1 1 1 1 1)
#packet_size=(1000)

### Exp: Equal stake RSMs of various sizes; message size 100.
#rsm1_size=(4 7 10)
#rsm2_size=(4 7 10)
#rsm1_fail=(1 2 3)
#rsm2_fail=(1 2 3)
#RSM1_Stake=(1 1 1 1 1 1 1 1 1 1)
#RSM2_Stake=(1 1 1 1 1 1 1 1 1 1)
#packet_size=(100)

### Exp: Equal stake RSMs of various sizes; varying message sizes.
#rsm1_size=(4 7 10)
#rsm2_size=(4 7 10)
#rsm1_fail=(1 2 3)
#rsm2_fail=(1 2 3)
#RSM1_Stake=(1 1 1 1 1 1 1 1 1 1)
#RSM2_Stake=(1 1 1 1 1 1 1 1 1 1)
#packet_size=(100 1000 10000 100000 1000000)

### Exp: Raft and ResilientDB; message size 100.
#rsm1_size=(4 7 10)
#rsm2_size=(3 5 7)
#rsm1_fail=(1 2 3)
#rsm2_fail=(1 2 3)
#RSM1_Stake=(1 1 1 1 1 1 1 1 1 1)
#RSM2_Stake=(1 1 1 1 1 1 1)
#packet_size=(100)

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

# TODO Change to inputs!!
GP_NAME="scrooge-micah-resdb"
ZONE="us-central1-a"
TEMPLATE="resdb-template"

function exit_handler() {
        echo "** Trapped CTRL-C, deleting experiment"
		#yes | gcloud compute instance-groups managed delete $GP_NAME --zone $ZONE
		exit 1
}

trap exit_handler INT
yes | gcloud beta compute instance-groups managed create "${GP_NAME}" --project=scrooge-398722 --base-instance-name="${GP_NAME}" --size="$((num_nodes_rsm_1+num_nodes_rsm_2+client))" --template=projects/scrooge-398722/global/instanceTemplates/${TEMPLATE} --zone="${ZONE}" --list-managed-instances-results=PAGELESS --stateful-internal-ip=interface-name=nic0,auto-delete=never --no-force-update-on-repair > /dev/null 2>&1

rm /tmp/all_ips.txt
num_ips_read=0
while ((${num_ips_read} < $((num_nodes_rsm_1+num_nodes_rsm_2+client)))); do
	gcloud compute instances list --filter="name~^${GP_NAME}" --format='value(networkInterfaces[0].networkIP)' > /tmp/all_ips.txt
	output=$(cat /tmp/all_ips.txt)
	ar=($output)
	num_ips_read="${#ar[@]}"
done

RSM1=(${ar[@]::${num_nodes_rsm_1}})
RSM2=(${ar[@]:${num_nodes_rsm_2}:${num_nodes_rsm_2}})
CLIENT=(${ar[@]:${num_nodes_rsm_1}+${num_nodes_rsm_2}:${client}})

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

# TODO: Change if networkurls script fails to copy
sleep 300
echo "Starting Experiment"

makeExperimentJson() {
	r1size=$1
	r2size=$2
	r1fail=$3
	r2fail=$4
	pktsize=$5
	expName=$6

	echo -e "{" >experiments.json
	echo -e "  \"experiment_independent_vars\": {" >>experiments.json
	echo -e "    \"project_dir\": \"${workdir}/BFT-RSM/Code/experiments/results/\"," >>experiments.json
	echo -e "    \"src_dir\": \"${workdir}/BFT-RSM/Code/\"," >>experiments.json
	echo -e "    \"network_dir\": \"${network_dir}\"," >>experiments.json
	echo -e "    \"exec_dir\": \"${exec_dir}\"," >>experiments.json
	echo -e "    \"ssh_key\": \"${key_file}\"," >>experiments.json
	echo -e "    \"username\": \"${username}\"," >>experiments.json
	echo -e "    \"local_setup_script\": \"${workdir}/BFT-RSM/Code/setup-seq.sh\"," >>experiments.json
	echo -e "    \"remote_setup_script\": \"${workdir}/BFT-RSM/Code/setup_remote.sh\"," >>experiments.json
	echo -e "    \"local_compile_script\": \"${workdir}/BFT-RSM/Code/build.sh\"," >>experiments.json
	echo -e "    \"replication_protocol\": \"scrooge\"," >>experiments.json

	echo -e "    \"clusterZeroIps\": [" >>experiments.json
	lcount=0
	while ((lcount < r1size)); do
		echo -e -n "      \"${RSM1[$lcount]}\"" >>experiments.json
		lcount=$((lcount + 1))
		if [ ${lcount} -eq "${r1size}" ]; then
			break
		fi
		echo -e "," >>experiments.json
	done
	echo "" >>experiments.json
	echo -e "    ]," >>experiments.json

	echo -e "    \"clusterOneIps\": [" >>experiments.json
	lcount=0
	while ((lcount < r2size)); do
		echo -e -n "      \"${RSM2[$lcount]}\"" >>experiments.json
		lcount=$((lcount + 1))
		if [ ${lcount} -eq "${r2size}" ]; then
			break
		fi
		echo -e "," >>experiments.json
	done
	echo "" >>experiments.json
	echo -e "    ]," >>experiments.json

	echo -e "    \"client_regions\": [" >>experiments.json
	echo -e "      \"US\"" >>experiments.json
	echo -e "    ]" >>experiments.json
	echo -e "  }," >>experiments.json

	echo -e "  \"${expName}\": {" >>experiments.json
	echo -e "    \"experiment_name\": \"increase_packet_size\"," >>experiments.json
	echo -e "    \"nb_rounds\": 1," >>experiments.json
	echo -e "    \"simulate_latency\": 0," >>experiments.json
	echo -e "    \"duration\": 4," >>experiments.json
	echo -e "    \"scrooge_args\": {" >>experiments.json
	echo -e "      \"general\": {" >>experiments.json
	echo -e "        \"use_debug_logs_bool\": 0," >>experiments.json
	echo -e "        \"log_path\": \"${log_dir}\"," >>experiments.json
	echo -e "        \"max_num_packets\": ${num_packets}" >>experiments.json
	echo -e "      }," >>experiments.json
	echo -e "      \"cluster_0\": {" >>experiments.json
	echo -e "        \"local_num_nodes\": [" >>experiments.json
	echo -e "          ${r1size}" >>experiments.json
	echo -e "        ]," >>experiments.json
	echo -e "        \"foreign_num_nodes\": [" >>experiments.json
	echo -e "          ${r2size}" >>experiments.json
	echo -e "        ]," >>experiments.json
	echo -e "        \"local_max_nodes_fail\": [" >>experiments.json
	echo -e "          ${r1fail}" >>experiments.json
	echo -e "        ]," >>experiments.json
	echo -e "        \"foreign_max_nodes_fail\": [" >>experiments.json
	echo -e "          ${r2fail}" >>experiments.json
	echo -e "        ]," >>experiments.json
	echo -e "        \"num_packets\": [" >>experiments.json
	echo -e "          ${num_packets}" >>experiments.json
	echo -e "        ]," >>experiments.json
	echo -e "        \"packet_size\": [" >>experiments.json
	echo -e "          ${pktsize}" >>experiments.json
	echo -e "        ]" >>experiments.json
	echo -e "      }," >>experiments.json
	echo -e "      \"cluster_1\": {" >>experiments.json
	echo -e "        \"local_num_nodes\": [" >>experiments.json
	echo -e "          ${r2size}" >>experiments.json
	echo -e "        ]," >>experiments.json
	echo -e "        \"foreign_num_nodes\": [" >>experiments.json
	echo -e "          ${r1size}" >>experiments.json
	echo -e "        ]," >>experiments.json
	echo -e "        \"local_max_nodes_fail\": [" >>experiments.json
	echo -e "          ${r2fail}" >>experiments.json
	echo -e "        ]," >>experiments.json
	echo -e "        \"foreign_max_nodes_fail\": [" >>experiments.json
	echo -e "          ${r1fail}" >>experiments.json
	echo -e "        ]," >>experiments.json
	echo -e "        \"num_packets\": [" >>experiments.json
	echo -e "          ${num_packets}" >>experiments.json
	echo -e "        ]," >>experiments.json
	echo -e "        \"packet_size\": [" >>experiments.json
	echo -e "          ${pktsize}" >>experiments.json
	echo -e "        ]" >>experiments.json
	echo -e "      }" >>experiments.json
	echo -e "    }" >>experiments.json
	echo -e "  }" >>experiments.json
	echo -e "}" >>experiments.json

	cp experiments.json ${json_dir} #copy to the expected folder.
}

#
# CREATING Config.h and network files.
#

rcount=0
protocols=()
if [ "${scrooge}" = "true" ]; then
	protocols+=("scrooge")
fi
if [ "${all_to_all}" = "true" ]; then
	protocols+=("all_to_all")
fi
if [ "${one_to_one}" = "true" ]; then
	protocols+=("one_to_one")
fi

for r1_size in "${rsm1_size[@]}"; do # Looping over all the network sizes
	# First, we create the configuration file "network0urls.txt" through echoing and redirection.
	rm -f network0urls.txt
	count=0
	while ((count < r1_size)); do
		echo -n "${RSM1[$count]}" >>network0urls.txt
		echo -n " " >>network0urls.txt
		echo "${RSM1_Stake[$count]}" >>network0urls.txt
		count=$((count + 1))
	done
	cat network0urls.txt
	cp network0urls.txt ${network_dir} #copy to the expected folder.
	echo " "

	# Next, we create the configuration file "network1urls.txt" through echoing and redirection.
	rm -f network1urls.txt
	count=0
	while ((${count} < ${rsm2_size[$rcount]})); do
		echo -n "${RSM2[$count]}" >>network1urls.txt
		echo -n " " >>network1urls.txt
		echo "${RSM2_Stake[$count]}" >>network1urls.txt
		count=$((count + 1))
	done
	cat network1urls.txt
	cp network1urls.txt ${network_dir} #copy to the expected folder.
	echo " "

	# scp network files to expected directory on other machines
	count=0
	r2size=${rsm2_size[$rcount]}
	parallel -v --jobs=0 scp -oStrictHostKeyChecking=no -i "${key_file}" ${network_dir}{1} ${username}@{2}:"${exec_dir}" ::: network0urls.txt network1urls.txt ::: "${RSM1[@]:0:$r1_size}"
	parallel -v --jobs=0 scp -oStrictHostKeyChecking=no -i "${key_file}" ${network_dir}{1} ${username}@{2}:"${exec_dir}" ::: network0urls.txt network1urls.txt ::: "${RSM2[@]:0:$r2size}"

	############# Setup all necessary external applications #############
	function start_raft() {
		echo "Raft RSM is being used!"
		# Take in arguments
		local cluster_num=$1
		local size=$2
		local RSM=("${!3}")
		# TODO - SETUP RAFT (ingest correct IPs)
	}

	# Setup all necessary external applications
	function start_algorand() {
		echo "Algorand RSM is being used!"
		# Take in arguments
		local client_ip=$1
		local size=$2
		local RSM=("${!3}")

		genesis_json=${algorand_scripts_dir}/genesis.json;
		per_node_algos=${starting_algos}/${rsm_size};
		mkdir ./genesis_creation/
		cp $genesis_json ./genesis_creation/
		mkdir ./addresses/
		#Relay nodes
		ssh -o StrictHostKeyChecking=no -t "${RSM[0]}" '${algorand_script_dir}/setup_algorand_nodes.py ${algorand_app_dir} ${algorand_script_dir} ${algorand_script_dir}/relay_config.json ${per_node_algos}';
		#Participation nodes
		parallel -v --jobs=0 ssh -o StrictHostKeyChecking=no -t {1} '${algorand_script_dir}/setup_algorand_nodes.py ${algorand_app_dir} ${algorand_script_dir} ${algorand_script_dir}/node_config.json ${per_node_algos}' ::: "${RSM[@]:1:$((size-1))}";
		
		### Get genesis files ###
		parallel -v --jobs=0 scp -oStrictHostKeyChecking=no -i "${key_file}" ${username}@{1}:~/{1}_gen.json ./genesis_creation/::: "${RSM[@]:0:$((size-1))}"
		# Combine genesis pieces into one file
		count=0
		while ((count < r1_size)); do
			echo $(genesis=$(jq 'select(has("addr"))' ./genesis_creation/${RSM[$count]}_gen.json);jq --argjson genesis "$genesis" '.alloc += [ $genesis ]' genesis.json) > genesis.json
			count=$((count + 1))
		done
		# Copy final genesis files onto all machines
		parallel -v --jobs=0 scp -oStrictHostKeyChecking=no -i "${key_file}" ./genesis_creation/genesis.json ${username}@{1}:${algorand_script_dir}/node/ ::: "${RSM[@]:0:$((size-1))}"

		### Get address matchings ###
		parallel -v --jobs=0 scp -oStrictHostKeyChecking=no -i "${key_file}" ${username}@{1}:~/{1}_addr.json ./address_creation/::: "${RSM[@]:0:$((size-1))}"
		# Combine genesis pieces into one file
		## TODO??? How to make address matchings
		# Copy final genesis files onto all machines
		parallel -v --jobs=0 scp -oStrictHostKeyChecking=no -i "${key_file}" ./addr_creation/{1}_node.json ${username}@{1}:${algorand_app_dir}/wallet_app/node.json ::: "${RSM[@]:0:$((size-1))}"

		# Remove genesis + address directories
		rm -rf ./genesis_creation
		rm -rf ./addresses

		# Finish wallet setup + run Algorand - TODO STILL UNFINISHED
		parallel -v --jobs=0 ssh -o StrictHostKeyChecking=no -t {1} '${algorand_script_dir}/run_algorand_nodes.py ${genesis_json} ${algorand_app_dir} ${algorand_script_dir} ${per_node_algos} ${client_ip}' ::: "${RSM[@]:1:$rsm_size}";
	}
	
	function start_resdb() {
		echo "ResDB RSM is being used!"
		# Take in arguments
		local cluster_num=$1
		local size=$2
		local RSM=("${!3}")
		# Create a new kv server conf file
		rm ${resdb_app_dir}/deploy/config/kv_performance_server.conf
		printf "%s\n" "iplist=(" >> ${resdb_app_dir}/deploy/config/kv_performance_server.conf
		count=0
		while ((${count} < ${size})); do
				printf "%s\n" "${RSM[$count]}" >> ${resdb_app_dir}/deploy/config/kv_performance_server.conf
				count=$((count + 1))
		done
		printf "%s\n\n" ")" >> ${resdb_app_dir}/deploy/config/kv_performance_server.conf
		echo "server=//kv_server:kv_server_performance" >> ${resdb_app_dir}/deploy/config/kv_performance_server.conf
		echo "HERE IS THE KV CONFIG:"	
		cat ${resdb_app_dir}/deploy/config/kv_performance_server.conf		
		
	 	# Create a new kv client conf file
		rm ${resdb_app_dir}/deploy/config_out/client.conf
		echo "${CLIENT[$cluster_num-1]}"
		num_nodes=$((size + 1))
		printf "\n%s" "${num_nodes} ${CLIENT[$cluster_num-1]} 17005" >> ${resdb_app_dir}/deploy/config_out/client.conf
		echo "HERE IS THE KV CLIENT CONFIG:"
		cat ${resdb_app_dir}/deploy/config_out/client.conf	
		
		# Run startup script
		${resdb_scripts_dir}/scrooge-resdb.sh ${resdb_app_dir} $cluster_num ${resdb_scripts_dir}
		sleep 120 # Sleeping to make sure resdb has had a chance to start
	}

	function start_raft() {
		echo "Raft RSM is being used!"
	}

	# Sending RSM
	#if [ "$send_rsm" = "algo" ]; then
	#	start_algorand(${RSM1}, $r1_size)
	if [ "$send_rsm" = "resdb" ]; then
		echo "ResDB RSM is being used for sending."
		cluster_idx=1
		start_resdb "${cluster_idx}" "${r1_size}" "RSM1[@]"
	elif [ "$send_rsm" = "raft" ]; then
		echo "Raft RSM is being used for sending."
	#	start_raft(${RSM1}, $r1_size)
	elif [ "$send_rsm" = "file" ]; then
		echo "File RSM is being used for sending. No extra setup necessary."
	else
		echo "INVALID RECEIVING RSM."
	fi

	# Receiving RSM
	#if [ "$receive_rsm" = "algo" ]; then
	#	echo "Algo RSM is being used for receiving."
	#	start_algorand(${RSM2}, $r2_size)
	if [ "$receive_rsm" = "resdb" ]; then
		echo "ResDB RSM is being used for receiving."
		cluster_idx=2
		start_resdb "${cluster_idx}" "${r1_size}" "RSM2[@]"
	elif [ "$receive_rsm" = "raft" ]; then
		echo "Raft RSM is being used for receiving."
	#	start_raft(${RSM2}, $r2_size)
	elif [ "$receive_rsm" = "file" ]; then
		echo "File RSM is being used for receiving. No extra setup necessary."
	else
		echo "INVALID RECEIVING RSM."
	fi

	for algo in "${protocols[@]}"; do # Looping over all the protocols.
		scrooge="false"
		all_to_all="false"
		one_to_one="false"

		if [ "${algo}" = "scrooge" ]; then
			scrooge="true"
		elif [ "${algo}" = "all_to_all" ]; then
			all_to_all="true"
		else
			one_to_one="true"
		fi
		for kl_size in "${klist_size[@]}"; do                   # Looping over all the klist_sizes.
			for pk_size in "${packet_size[@]}"; do                 # Looping over all the packet sizes.
				for bt_size in "${batch_size[@]}"; do                 # Looping over all the batch sizes.
					for bt_create_tm in "${batch_creation_time[@]}"; do  # Looping over all batch creation times.
						for pl_buf_size in "${pipeline_buffer_size[@]}"; do # Looping over all pipeline buffer sizes.
							# Next, we call the script that makes the config.h. We need to pass all the arguments.
							./makeConfig.sh "${r1_size}" "${rsm2_size[$rcount]}" "${rsm1_fail[$rcount]}" "${rsm2_fail[$rcount]}" ${num_packets} "${pk_size}" ${network_dir} ${log_dir} ${warmup_time} ${total_time} "${bt_size}" "${bt_create_tm}" ${max_nng_blocking_time} "${pl_buf_size}" ${message_buffer_size} "${kl_size}" ${scrooge} ${all_to_all} ${one_to_one} ${file_rsm} ${use_debug_logs_bool}

							cat config.h
							cp config.h system/

							make clean
							make proto
							make -j scrooge

							# Next, we make the experiment.json for backward compatibility.
							makeExperimentJson "${r1_size}" "${rsm2_size[$rcount]}" "${rsm1_fail[$rcount]}" "${rsm2_fail[$rcount]}" "${pk_size}" ${experiment_name}

							# Next, we run the script.
							./experiments/experiment_scripts/run_experiments.py ${workdir}/BFT-RSM/Code/experiments/experiment_json/experiments.json ${experiment_name}
						done
					done
				done
			done
		done
	done
	rcount=$((rcount + 1))
done

echo "taking down experiment"
#yes | gcloud compute instance-groups managed delete $GP_NAME --zone $ZONE

############# DID YOU DELETE THE MACHINES?????????????????


