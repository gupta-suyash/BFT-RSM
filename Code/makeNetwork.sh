#!/bin/bash
## Created By: Suyash Gupta - 08/23/2023
## Has the most updated algorand, resdb functions (useExistingIP, this branch) and raft (commit 68bb9b0 on application-testing branch)
## This script helps to create the files that specify URLs for RSM1 and RSM2. Additionally, it calls the script that helps to create "config.h". We need to specify the URLs and stakes for each node in both the RSMs. This script takes in argument the size of both the RSMs and other necessary parameters.

if [ -z ${TMUX+x} ]; then
	echo "Run script in tmux to guarantee progress"
	echo "exiting..."
	exit 1
fi

echo -n "Enter the name of the experiment being run: "

read experiment_name

echo "Running Experiment: ${experiment_name}"


echo -n "Are you using static machines? (T or F) "

read static_machines

echo "You have said ${static_machines} to using static machines."
echo "NOTE: STATIC MACHINES ONLY WORK WITH 4 NODE CLUSTERS!!!"
# If this experiment uses external applications, set the following values
# Valid inputs: "algo", "resdb", "raft"
# e.x. if algorand is the sending RSM then send_rsm="algo", if resdb is
# receiving RSM, then receive_rsm="resdb"
echo -n "Enter the name of the sending application (4 options: algo, resdb, raft, file): "
read send_rsm
echo "Sending Application: ${send_rsm}"

echo -n "Enter the name of the receiving application (4 options: algo, resdb, raft, file): "
read receive_rsm
echo "Receiving Application: ${receive_rsm}"

echo -n "Are you rerunning an application? Only applies to algo-algo. (T of F): "
read rerun_bool
echo "You have chosen ${rerun_bool} for rerunning algo-algo experiment."

#If this experiment is for File_RSM (not algo or resdb)
file_rsm="true"
if [ "$send_rsm" != "file" ] || [ "$receive_rsm" != "file" ]; then
    file_rsm="false"
fi
#file_rsm="true" # TODO TODO TAKE THIS OUT!!!
#echo "ALWAYS RUNNING FILE RSM!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"

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
scrooge="true"
all_to_all="false"
one_to_one="false"
geobft="false" # "true"
leader="false"
#If this experiment is for File_RSM (not algo or resdb)
# If this experiment uses external applications, set the following values
# Valid inputs: "algo", "resdb", "raft", "file"
# e.x. if algorand is the sending RSM then send_rsm="algo", if resdb is
# receiving RSM, then receive_rsm="resdb"
#send_rsm="raft"
#receive_rsm="raft"
echo "Send rsm: "
echo $send_rsm
echo "Receive rsm: "
echo $receive_rsm

if [ "$file_rsm" = "false" ]; then
	echo "WARNING: FILE RSM NOT BEING USED"
fi

echo "The applications you are running are $send_rsm and $receive_rsm." 
# echo -n "Please acknolwedge and accept/reject. Type Y or N: "

# read application_acknowledgement

# if [ $application_acknowledgement != "Y" ]; then
# 	echo "Please check the application you are running."
# 	echo "Otherwise you will be sad :') Exiting now..."
# 	exit 1
# fi

### DUMMY Exp: Equal stake RSMs of size 4; message size 100.
#rsm1_size=(19)
#rsm2_size=(19)
#rsm1_fail=(6 7)
#rsm2_fail=(6 7)
#RSM1_Stake=(1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1)
#RSM2_Stake=(1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1)
#klist_size=(64)
#packet_size=(1000000)
#batch_size=(200000)
#batch_creation_time=(1ms)
#pipeline_buffer_size=(8)

rsm1_size=(7 13 16 19)
rsm2_size=(7 13 16 19)
rsm1_fail=(2 4 5 6)
rsm2_fail=(2 4 5 6)
RSM1_Stake=(1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1)
RSM2_Stake=(1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1)
klist_size=(0 64)
packet_size=(1000000)
batch_size=(200000)
batch_creation_time=(1ms)
pipeline_buffer_size=(8)
noop_delays=(.8ms 1ms 12ms 100ms)
max_message_delays=(.8ms 1ms 12ms 100ms)
quack_windows=(100 500 1000 2000)
ack_windows=(10 30 100 500 1000)


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

echo "SET RSM SIZES"
echo "$num_nodes_rsm_1"
echo "$num_nodes_rsm_2"
# TODO Change to inputs!!
GP_NAME="$experiment_name"
echo "$GP_NAME"
ZONE="us-west1-b"
TEMPLATE="updated-app-template" # NOTE: Look at the algo-timing template, might be necessary to run applications

WORKING_DIR_CLEAN="TRUE"
if output=$(git status --porcelain) && [ -z "$output" ]; then
  echo "Working Directory is clean!"
  WORKING_DIR_CLEAN="TRUE"
else 
  WORKING_DIR_CLEAN="FALSE"
  git stash --include-untracked
  git stash apply
  git switch -c "AUTOMATED_BRANCH/$(date +"%Y-%m-%d_%H-%M-%S")/${GP_NAME}/${experiment_name}"
  git add .
  git commit -m "Experiment Generated Commit $(date +"%Y-%m-%d_%H-%M-%S")/${GP_NAME}/${experiment_name}"
  git push -u origin HEAD
fi

function exit_handler() {
	echo "** Trapped CTRL-C, deleting experiment"
	if [ "${WORKING_DIR_CLEAN}" = "FALSE" ]; then
	  git reset --hard HEAD
	  git switch -
	  git stash pop
	fi
	yes | gcloud compute instance-groups managed delete $GP_NAME --zone $ZONE
	exit 1
}

trap exit_handler INT
echo "Create group name"
echo "${GP_NAME}"
echo "$((num_nodes_rsm_1+num_nodes_rsm_2+client))"
echo "${ZONE}"
echo "${TEMPLATE}"
RSM1=()
RSM2=()
CLIENT=()
if [ $static_machines = "F" ]; then
    echo "No static machines, creating cluster!"
    yes | gcloud beta compute instance-groups managed create "${GP_NAME}" --project=scrooge-398722 --base-instance-name="${GP_NAME}" --size="$((num_nodes_rsm_1+num_nodes_rsm_2+client))" --template=projects/scrooge-398722/global/instanceTemplates/${TEMPLATE} --zone="${ZONE}" --list-managed-instances-results=PAGELESS --stateful-internal-ip=interface-name=nic0,auto-delete=never --no-force-update-on-repair --default-action-on-vm-failure=repair
    #> /dev/null 2>&1
    sleep 50
    rm /tmp/all_ips.txt
    num_ips_read=0
    while ((${num_ips_read} < $((num_nodes_rsm_1+num_nodes_rsm_2+client)))); do
	    gcloud compute instances list --filter="name~^${GP_NAME}" --format='value(networkInterfaces[0].networkIP)' > /tmp/all_ips.txt
	    output=$(cat /tmp/all_ips.txt)
	    ar=($output)
	    num_ips_read="${#ar[@]}"
    done
    # TODO: Change this back or it'll be confusing (also change in algo function and in scrooge for loop)
    # && sudo wondershaper ens4 2000000 2000000'
    #parallel -v --jobs=0 ssh -o StrictHostKeyChecking=no -t {1} 'sudo wondershaper clean ens4' ::: "${ar[@]:0:19}";
    #parallel -v --jobs=0 ssh -o StrictHostKeyChecking=no -t {1} 'sudo wondershaper clean ens4' ::: "${ar[@]:19:19}";
    #parallel -v --jobs=0 ssh -o StrictHostKeyChecking=no -t {1} 'sudo apt remove wondershaper -y' ::: "${ar[@]:0:19}";
    #parallel -v --jobs=0 ssh -o StrictHostKeyChecking=no -t {1} 'sudo apt remove wondershaper -y' ::: "${ar[@]:19:19}";
    #parallel -v --jobs=0 ssh -o StrictHostKeyChecking=no -t {1} 'sudo tc qdisc add dev ens4 root tbf rate 1gbit burst 1mbit latency .5ms' ::: "${ar[@]:1:18}";
    #parallel -v --jobs=0 ssh -o StrictHostKeyChecking=no -t {1} 'sudo tc qdisc add dev ens4 root tbf rate 1gbit burst 1mbit latency .5ms' ::: "${ar[@]:20:18}";
    #sudo tc qdisc add dev eth0 root tbf rate 1mbit burst 64kbit latency 400ms0
    RSM1=(${ar[@]::${num_nodes_rsm_1}})
    RSM2=(${ar[@]:${num_nodes_rsm_2}:${num_nodes_rsm_2}})
    CLIENT=(${ar[@]:${num_nodes_rsm_1}+${num_nodes_rsm_2}:${client}})
    echo "About to parallel!"
    #parallel --dryrun -v --jobs=0 echo {1} ::: "${RSM1[@]:0:$((num_nodes_rsm_1-1))}";
else 
    echo "Have static IPs!"
    RSM1=(10.128.7.13 10.128.7.14 10.128.7.15 10.128.7.16)
    RSM2=(10.128.7.18 10.128.7.19 10.128.7.20 10.128.7.21)
    CLIENT=(10.128.7.17 10.128.7.22) # Machines: 5 and 10
fi
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

#sleep 300
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
if [ "${geobft}" = "true" ]; then
    protocols+=("geobft")
fi
if [ "${leader}" = "true" ]; then
    protocols+=("leader")
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
	cp network0urls.txt ${network_dir} #cops to the expected folder.
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
	joinedvar1=""
	joinedvar2=""
	raft_count=1
	function start_raft() {
		echo "Raft RSM is being used!"
		# Take in arguments
		local client_ip=$1
		local size=$2
		local RSM=("${!3}")
		etcd_path="${raft_app_dir}etcd-main/"
    		# Run setup build script
           	#Client node
		ssh -i ${key_file} -o StrictHostKeyChecking=no -t "${client_ip}" 'cd '"${etcd_path}"' && export PATH=$PATH:/usr/local/go/bin && '"${etcd_path}"'scripts/build.sh'
           	echo "Sent build information!"
           	#Server nodes
           	#parallel -v --jobs=0 ssh -o StrictHostKeyChecking=no -t {1} 'pwd && cd '"${etcd_path}"' && pwd && export PATH=$PATH:/usr/local/go/bin && '"${etcd_path}"'scripts/build.sh' ::: "${RSM[@]:0:$((size))}";
		for i in ${!RSM[@]}; do
			echo "building etcd on RSM: ${RSM[$i]}"
			ssh -i ${key_file} -o StrictHostKeyChecking=no ${RSM[$i]} "export PATH=\$PATH:/usr/local/go/bin; cd ${etcd_path}; echo \$(pwd); ./scripts/build.sh; exit"
		done
		# Set constants
		
		etcd_bin_path="${etcd_path}bin"
		benchmark_bin_path="${raft_app_dir}bin"
		echo "etcd bin path: ${etcd_bin_path}"
		echo "benchmark bin path: ${benchmark_bin_path}"
		TOKEN=token-99
		CLUSTER_STATE=new
		count=0
		machines=()
		urls=()
		cluster=()
		rsm_w_ports=()
		while ((${count} < ${size})); do
			echo "RSM: ${RSM[$count]}"
			echo "count: ${count}, size: ${size}"
			machines+=("machine-$((count + 1))")
			url+=(http://"${RSM[$count]}":2380)
			cluster+=("machine-$((count + 1))"=http://"${RSM[$count]}":2380)
			rsm_w_ports+=("${RSM[$count]}:2379")
			count=$((count + 1))
		done
		# Run the first etcd cluster
		printf -v cluster_list '%s,' "${cluster[@]}"
		#echo export "PATH=\$PATH:${benchmark_bin_path}:${etcd_bin_path}" >> $HOME/.bashrc
		for i in ${!RSM[@]}; do
			this_name=${machines[$i]}
			this_ip=${RSM[$i]}
			this_url=${urls[$i]}
			#scp -o StrictHostKeyChecking=no $HOME/.bashrc ${username}@${this_ip}:$HOME/

			(ssh -i ${key_file} -o StrictHostKeyChecking=no ${RSM[$i]} "export THIS_NAME=${this_name}; export THIS_IP=${this_ip}; export TOKEN=${TOKEN}; export CLUSTER_STATE=${CLUSTER_STATE}; export CLUSTER="${cluster_list%,}"; cd \$HOME; echo PWD: \$(pwd)  THIS_NAME:\${THIS_NAME} THIS_IP:\${THIS_IP} TOKEN:\${TOKEN} CLUSTER:\${CLUSTER}; echo \"before kilall\"; killall -9 benchmark; echo \"after kilall\"; sudo fuser -n tcp -k 2379 2380; sudo rm -rf \$HOME/data.etcd; echo \$HOME/.bashrc; ${etcd_bin_path}/etcd --data-dir=data.etcd --name \${THIS_NAME} --initial-advertise-peer-urls http://\${THIS_IP}:2380 --listen-peer-urls http://\${THIS_IP}:2380 --advertise-client-urls http://\${THIS_IP}:2379 --listen-client-urls http://\${THIS_IP}:2379 --initial-cluster \${CLUSTER} --initial-cluster-state \${CLUSTER_STATE} --initial-cluster-token \${TOKEN} &> background_raft_\${THIS_IP}.log") &
		done
		printf -v joined '%s,' "${rsm_w_ports[@]}"
		echo "RSM w ports: ${joined%,}"
		# Start benchmark
    		export PATH=$PATH:${benchmark_bin_path}
		
		if [ "${raft_count}" -eq 1 ]; then
			joinedvar1="${joined%,}"
			echo "RSM1: ${joinedvar1}"
			raft_count=2
		else
			joinedvar2="${joined%,}"
			echo "RSM2: ${joinedvar2}"
		fi
	}

	function benchmark_raft() {
		local joinedvar=$1
		local raft_count=$2
		echo "IN BENCHMARK_RAFT ${joinedvar}"
		echo "" > benchmark_${raft_count}.log
		for i in {1..3}; do
			benchmark --endpoints="${joinedvar}" --conns=100 --clients=1000 put --key-size=8 --sequential-keys --total=900000 --val-size=100 &>> benchmark_${raft_count}.log
		done		
	}
	
	function start_algorand() {
		echo "######################################################Algorand RSM is being used!"
		# Take in arguments
		local client_ip=$1
		local size=$2
        local pk_file=$3
		local RSM=("${!4}")
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
        # Delete old log file to preserve correctness
        ssh -o StrictHostKeyChecking=no -t "${client_ip}" 'rm -rf '"${algorand_scripts_dir}"'/genesis_creation && rm -rf '"${algorand_scripts_dir}"'/addresses && mkdir '"${algorand_scripts_dir}"'/genesis_creation/ && mkdir '"${algorand_scripts_dir}"'/addresses/'
        parallel -v --jobs=0 ssh -o StrictHostKeyChecking=no -t {1} 'rm -rf '"${algorand_scripts_dir}"'/genesis_creation && rm -rf '"${algorand_scripts_dir}"'/addresses && mkdir '"${algorand_scripts_dir}"'/genesis_creation/ && mkdir '"${algorand_scripts_dir}"'/addresses/' ::: "${RSM[@]:0:$((size))}";
		echo "Copy over message payload and node.go file!"
        # Copy over message payload, passed in as an argument - TODO need to change name of payload file
        scp -o StrictHostKeyChecking=no -i "${key_file}" ${workdir}/BFT-RSM/Code/${pk_file} ${username}@${client_ip}:${workdir}/1M_byte_payload.txt
        parallel -v --jobs=0 scp -o StrictHostKeyChecking=no -i "${key_file}" ${workdir}/BFT-RSM/Code/${pk_file} ${username}@{1}:${workdir}/1M_byte_payload.txt ::: "${RSM[@]:0:$((size))}";
        # Copy over node.go file
        scp -o StrictHostKeyChecking=no -i "${key_file}" ${algorand_app_dir}/node/node.go ${username}@${client_ip}:${algorand_app_dir}/node/
        parallel -v --jobs=0 scp -o StrictHostKeyChecking=no -i "${key_file}" ${algorand_app_dir}/node/node.go ${username}@{1}:${algorand_app_dir}/node/ ::: "${RSM[@]:0:$((size))}";
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

		### Finish running Algorand
		echo "###########################################FINISH RUNNING ALGORAND"
        #Relay nodes
        relay="true"
		ssh -o StrictHostKeyChecking=no -t "${client_ip}" ''"${algorand_scripts_dir}"'/run_relay_algorand.py '"${algorand_app_dir}"' '"${algorand_scripts_dir}"' '"${client_ip}"' '"${relay}"'' &
		echo "Relay node is run!"
        #Participation nodes
        relay="false"
		parallel -v --jobs=0 'ssh -o StrictHostKeyChecking=no -t {1} '''"${algorand_scripts_dir}"'/run_algorand.py '"${algorand_app_dir}"' '"${algorand_scripts_dir}"' '"${client_ip}"' '"${relay}"''' &' ::: "${RSM[@]:0:$((size))}";
        echo "###########################################Algorand started and running!"
	}

    function rerun_algorand() {
		echo "######################################################Algorand RSM is being used!"
		# Take in arguments
		local client_ip=$1
		local size=$2
        local pk_file=$3
		local RSM=("${!4}")
		echo "${RSM[@]}"
        echo "###########################################FINISH RUNNING ALGORAND"
        # Step 0: Copy wallet testing file onto each machine
        echo "Copy new wallet testing file!"
        scp -o StrictHostKeyChecking=no -i "${key_file}" ${algorand_app_dir}/wallet_app/wallet_test.js ${username}@${client_ip}:${algorand_app_dir}/wallet_app/
        parallel -v --jobs=0 scp -o StrictHostKeyChecking=no -i "${key_file}" ${algorand_app_dir}/wallet_app/wallet_test.js ${username}@{1}:${algorand_app_dir}/wallet_app/ ::: "${RSM[@]:0:$((size))}";
        # Step 0.5: Copy new payload onto each machine
        # NOTE: This is currently very unintuitive due to the default file name starting with 1M. TODO: Change name (must chande node.go)
        scp -o StrictHostKeyChecking=no -i "${key_file}" ${workdir}/BFT-RSM/Code/${pk_file} ${username}@${client_ip}:${workdir}/1M_byte_payload.txt
        parallel -v --jobs=0 scp -o StrictHostKeyChecking=no -i "${key_file}" ${workdir}/BFT-RSM/Code/${pk_file} ${username}@{1}:${workdir}/1M_byte_payload.txt ::: "${RSM[@]:0:$((size))}";
        # Step 1: Copy rerun script onto each machine
        scp -o StrictHostKeyChecking=no -i "${key_file}" ${algorand_scripts_dir}/scripts/rerun.sh ${username}@${client_ip}:${workdir}
        parallel -v --jobs=0 scp -o StrictHostKeyChecking=no -i "${key_file}" ${algorand_scripts_dir}/scripts/rerun.sh ${username}@{1}:${workdir} ::: "${RSM[@]:0:$((size))}";

        # Step 2: Execute rerun script
        relay="true"
        ssh -o StrictHostKeyChecking=no -t "${client_ip}" '/home/scrooge/rerun.sh '"${algorand_app_dir}"' '"${algorand_scripts_dir}"' '"${client_ip}"':4161 default '"${relay}"''
        relay="false"
        parallel -v --jobs=0 ssh -o StrictHostKeyChecking=no -t {1} '/home/scrooge/rerun.sh '"${algorand_app_dir}"' '"${algorand_scripts_dir}"' '"${client_ip}"':4161 default '"${relay}"''' &' ::: "${RSM[@]:0:$((size))}";
        echo "Done with the parallel jobs!"        
        echo "###########################################Algorand started and running!"
	}
    
    # Algorand Startup Verion #3: Does not rerun anything, directly starts another experiment on the same algorand instance
    function start_no_rerun_algorand() {
        echo "######################################################Algorand RSM is being used!"
        # Take in arguments
        local client_ip=$1
        local size=$2
        local RSM=("${!3}")
        echo "${RSM[@]}"
        echo "###########################################FINISH RUNNING ALGORAND"
        #Relay nodes
        relay="true"
        ssh -o StrictHostKeyChecking=no -t "${client_ip}" ''"${algorand_scripts_dir}"'/run_relay_algorand.py '"${algorand_app_dir}"' '"${algorand_scripts_dir}"' '"${client_ip}"' '"${relay}"'' &
        echo "Relay node is run!"
        #Participation nodes
        relay="false"
        parallel -v --jobs=0 'ssh -o StrictHostKeyChecking=no -t {1} '''"${algorand_scripts_dir}"'/run_algorand.py '"${algorand_app_dir}"' '"${algorand_scripts_dir}"' '"${client_ip}"' '"${relay}"''' &' ::: "${RSM[@]:0:$((size))}";
        echo "###########################################Algorand started and running!"
    }

	function start_resdb() {
		echo "ResDB RSM is being used!"
		# Take in arguments
		local cluster_num=$1
		local size=$2
        local client_ip=$3
		local RSM=("${!4}")
        # Delete old log file to save space
        echo "Gonna delete log files!"
        ssh -o StrictHostKeyChecking=no -t "${client_ip}" 'rm kv_server_performance*.log'
        echo "Delete log files on client!"
        parallel -v --jobs=0 ssh -o StrictHostKeyChecking=no -t {1} 'rm kv_server_performance*.log' ::: "${RSM[@]:0:$((size))}";
		echo "Deleted old log files!"
        # Create a new kv server conf file
		rm ${resdb_app_dir}/deploy/config/kv_performance_server.conf
		printf "%s\n" "iplist=(" >> ${resdb_app_dir}/deploy/config/kv_performance_server.conf
		count=0
		while ((${count} < ${size})); do
				printf "%s\n" "${RSM[$count]}" >> ${resdb_app_dir}/deploy/config/kv_performance_server.conf
				count=$((count + 1))
		done
        printf "%s\n" "${client_ip}" >> ${resdb_app_dir}/deploy/config/kv_performance_server.conf
		printf "%s\n\n" ")" >> ${resdb_app_dir}/deploy/config/kv_performance_server.conf
		echo "server=//kv_server:kv_server_performance" >> ${resdb_app_dir}/deploy/config/kv_performance_server.conf
		echo "HERE IS THE KV CONFIG:"	
		cat ${resdb_app_dir}/deploy/config/kv_performance_server.conf		
		${resdb_scripts_dir}/scrooge-resdb.sh ${resdb_app_dir} $cluster_num ${resdb_scripts_dir}
        echo "Resdb is started!!"
        #exit 1
	}
    
  for algo in "${protocols[@]}"; do # Looping over all the protocols.
    scrooge="false"
    all_to_all="false"
    one_to_one="false"
    geobft="false"
    leader="false"

    if [ "${algo}" = "scrooge" ]; then
      scrooge="true"
    elif [ "${algo}" = "all_to_all" ]; then
      all_to_all="true"
    elif [ "${algo}" = "geobft" ]; then
       geobft="true"
    elif [ "${algo}" = "leader" ]; then

       leader="true"
    else
        one_to_one="true"
    fi
	for kl_size in "${klist_size[@]}"; do            # Looping over all the klist_sizes.
		for pk_size in "${packet_size[@]}"; do   # Looping over all the packet sizes.
			for bt_size in "${batch_size[@]}"; do     # Looping over all the batch sizes.
				for bt_create_tm in "${batch_creation_time[@]}"; do  # Looping over all batch creation times.
					for pl_buf_size in "${pipeline_buffer_size[@]}"; do # Looping over all pipeline buffer sizes.
            for noop_delay in "${noop_delays[@]}"; do
								for max_message_delay in "${max_message_delays[@]}"; do
									for quack_window in "${quack_windows[@]}"; do
										for ack_window in "${ack_windows[@]}"; do
				            # Next, we call the script that makes the config.h. We need to pass all the arguments.
				            # First, get payload file name
                            pk_file=""
                            if [ "$pk_size" -eq 100 ]; then # TODO
                                pk_file="100_byte_payload.txt"
                            elif [ "$pk_size" -eq 1000 ]; then
                                pk_file="1K_byte_payload.txt"
                            elif [ "$pk_size" -eq 10000 ]; then
                                pk_file="10K_byte_payload.txt"
                            elif [ "$pk_size" -eq 100000 ]; then
                                pk_file="100K_byte_payload.txt"
                            else
                                pk_file="1M_byte_payload.txt"
                            fi
                            # Start sending RSM
                        	if [ "$send_rsm" = "algo" ]; then
                        		if [ "$rerun_bool" = "T" ]; then
                                    rerun_algorand "${CLIENT[0]}" "$r1_size" "$pk_file" "RSM1[@]"
                                else
                                    start_algorand "${CLIENT[0]}" "$r1_size" "$pk_file" "RSM1[@]"
                                fi
                        	elif [ "$send_rsm" = "resdb" ]; then
                        		echo "ResDB RSM is being used for sending."
                        		cluster_idx=1
                        		start_resdb "${cluster_idx}" "${r1_size}" "${CLIENT[0]}" "RSM1[@]"
                        	elif [ "$send_rsm" = "raft" ]; then
                        		echo "Raft RSM is being used for sending."
                        		start_raft "${CLIENT[0]}" "$r1_size" "RSM1[@]"
                        	elif [ "$send_rsm" = "file" ]; then
                        		echo "File RSM is being used for sending. No extra setup necessary."
                        	else
                        		echo "INVALID RECEIVING RSM."
                        	fi
                            	
				            # Receiving RSM 
                            if [ "$receive_rsm" = "algo" ]; then
                        		echo "Algo RSM is being used for receiving."
                                if [ "$rerun_bool" = "T" ]; then
                                    rerun_algorand "${CLIENT[1]}" "$r1_size" "$pk_file" "RSM2[@]"
                                else
                                    start_algorand "${CLIENT[1]}" "$r1_size" "$pk_file" "RSM2[@]"
                                fi
                        	elif [ "$receive_rsm" = "resdb" ]; then
                        		echo "ResDB RSM is being used for receiving."
                        		cluster_idx=2
                        		start_resdb "${cluster_idx}" "${r1_size}" "${CLIENT[1]}" "RSM2[@]"
                        	elif [ "$receive_rsm" = "raft" ]; then
                        		echo "Raft RSM is being used for receiving."
                        		start_raft "${CLIENT[1]}" "$r1_size" "RSM2[@]"
                        	elif [ "$receive_rsm" = "file" ]; then
                        		echo "File RSM is being used for receiving. No extra setup necessary."
                        	else
                        		echo "INVALID RECEIVING RSM."
                        	fi
                        	#echo "THIS SCRIPT IS EXITING FOR NOW INSTEAD OF RUNNING SCROOGE"
                        	#echo "FEEL FREE TO CHANGE BUT CHECK WHO ELSE IS RUNNING SCROOGE CONCURRENTLY PLEASE!!!!"
                        	echo "THIS SCRIPT IS SLEEPING FOR 1 MINUTE ON LINE 589 BEFORE RUNNING SCROOGE - FEEL FREE TO CHANGE"
			#exit 1
				# TODO: uncomment from 812 to 825 	
				#./makeConfig.sh "${r1_size}" "${rsm2_size[$rcount]}" "${rsm1_fail[$rcount]}" "${rsm2_fail[$rcount]}" ${num_packets} "${pk_size}" ${network_dir} ${log_dir} ${warmup_time} ${total_time} "${bt_size}" "${bt_create_tm}" ${max_nng_blocking_time} "${pl_buf_size}" ${message_buffer_size} "${kl_size}" ${scrooge} ${all_to_all} ${one_to_one} ${geobft} ${leader} ${file_rsm} ${use_debug_logs_bool} ${noop_delay} ${max_message_delay} ${quack_window} ${ack_window}

				#cat config.h
				#cp config.h system/

				#make clean
				#make proto
				#make -j scrooge

				# Next, we make the experiment.json for backward compatibility.
				#makeExperimentJson "${r1_size}" "${rsm2_size[$rcount]}" "${rsm1_fail[$rcount]}" "${rsm2_fail[$rcount]}" "${pk_size}" ${experiment_name}
				#parallel -v --jobs=0 scp -oStrictHostKeyChecking=no -i "${key_file}" ${network_dir}{1} ${username}@{2}:"${exec_dir}" ::: network0urls.txt network1urls.txt ::: "${RSM1[@]:0:$r1_size}"
				#parallel -v --jobs=0 scp -oStrictHostKeyChecking=no -i "${key_file}" ${network_dir}{1} ${username}@{2}:"${exec_dir}" ::: network0urls.txt network1urls.txt ::: "${RSM2[@]:0:$r2size}"

				# Next, we run the script.
				./experiments/experiment_scripts/run_experiments.py ${workdir}/BFT-RSM/Code/experiments/experiment_json/experiments.json ${experiment_name} &
				if [ "$send_rsm" = "raft" ]; then
					sleep 32
					benchmark_raft "${joinedvar1}" 1
				fi
				if [ "$receive_rsm" = "raft" ]; then
					sleep 32
					benchmark_raft "${joinedvar2}" 2
				fi

										done
									done
								done
							done
						done
					done
				done
			done
		done
	done
	rcount=$((rcount + 1))
done

echo "taking down experiment"

###### UNDO
#yes | gcloud compute instance-groups managed delete $GP_NAME --zone $ZONE

############# DID YOU DELETE THE MACHINES?????????????????

if [ "${WORKING_DIR_CLEAN}" = "FALSE" ]; then
	git reset --hard HEAD
	git switch -
	git stash pop
fi

