#!/bin/bash
## Created By: Suyash Gupta - 08/23/2023
## Has the most updated algorand, resdb functions (useExistingIP, this branch) and raft (commit 68bb9b0 on application-testing branch)
## This script helps to create the files that specify URLs for RSM1 and RSM2. Additionally, it calls the script that helps to create "config.h". We need to specify the URLs and stakes for each node in both the RSMs. This script takes in argument the size of both the RSMs and other necessary parameters.

if [ "$#" -lt 13 ]; then
    echo "Usage: $0 experiment_name protocol send_rsm receive_rsm RSM_Stake rsm_size klist_size packet_size simulate_crash byz_mode throttle_file run_dr run_ccf"
    exit 1
fi


experiment_name="$1"
protocol="$2"
send_rsm="$3"
send_rsm=${send_rsm,,}
receive_rsm="$4"
receive_rsm=${receive_rsm,,}
RSM1_Stake=("$5" 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1)
RSM2_Stake=("$5" 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1)
rsm1_size=("$6")
rsm2_size=("$6")
klist_size=("$7")
packet_size=("$8")
simulate_crash="$9"
byz_mode="${10}"
throttle_file="${11}"
run_dr="${12}"
run_ccf="${13}"
batch_size=("${14}")
batch_creation_time=("${15}")
pipeline_buffer_size=("${16}")
noop_delays=("${17}")
max_message_delays=("${18}")
quack_windows=("${19}")
ack_windows=("${20}")
max_nng_blocking_time="${21}"
message_buffer_size="${22}"


rsm1_fail=("$(((rsm1_size[0] - 1) / 3))")
rsm2_fail=("$(((rsm1_size[0] - 1) / 3))")

if [ "$run_dr" = "true" ] || [ "$run_ccf" = "true" ]; then
	rsm1_fail=("$(((rsm1_size[0] - 1) / 2))")
	rsm2_fail=("$(((rsm1_size[0] - 1) / 2))")
fi

####
# Setting protocol var
scrooge="false"
all_to_all="false"
one_to_one="false"
geobft="false"
leader="false"
kafka="false"

if [ "$protocol" = "SCROOGE" ]; then
	scrooge="true"
fi
if [ "$protocol" = "ATA" ]; then
	all_to_all="true"
fi
if [ "$protocol" = "OST" ]; then
	one_to_one="true"
fi
if [ "$protocol" = "OTU" ]; then
	geobft="true"
fi
if [ "$protocol" = "LL" ]; then
	leader="true"
fi
if [ "$protocol" = "KAFKA" ]; then
	kafka="true"
fi

###


if [ "$run_dr" = "true" ] && [ "$run_ccf" = "true" ]; then
    echo "Incorrect configuration. DR and CCF are both set to run which is unsupported. Exiting."
    exit 1
fi

if [ "$run_dr" = "true" ] || [ "$run_ccf" = "true" ]; then
	if [ "$send_rsm" = "file" ] || [ "$receive_rsm" = "file" ]; then
		echo "Incorrect configuration. Cannot use file alongside Disaster Recovery or CCF. Exiting."
		exit 1
	fi
fi


valid_applications=("algo" "resdb" "raft" "file")
if [[ ! " ${valid_applications[*]} " =~ " $send_rsm " ]]; then
  echo "$send_rsm is an invalid option, exiting..."
  exit 1
fi
if [[ ! " ${valid_applications[*]} " =~ " $receive_rsm " ]]; then
  echo "$receive_rsm is an invalid option, exiting..."
  exit 1
fi

file_rsm="true"
if [ "$send_rsm" != "file" ] || [ "$receive_rsm" != "file" ]; then
    file_rsm="false"
fi

# Not automating atm
# if [[ "$send_rsm" == "algorand" || "$receive_rsm" == "algorand" ]]; then
#     echo -n "Are you rerunning an application? Only applies to algo-algo. (T of F): "
# 	read rerun_bool
# 	echo "You have chosen ${rerun_bool} for rerunning algo-algo experiment."
# else
# 	rerun_bool="F"
# fi



# Name of profile we are running out of
key_file="$HOME/.ssh/id_ed25519" # TODO: Replace with your ssh key
username="scrooge"               # TODO: Replace with your username

# Working Directory
workdir="/home/scrooge"

# Set rarely changing Scrooge parameters.
warmup_time=45s
total_time=60s
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
kafka_dir="${workdir}/kafka_2.13-3.7.0/"
use_debug_logs_bool="false"

# Set rarely changing experiment application parameters
starting_algos=10000000000000000


pids_to_kill=()


# Build the network from the description
num_nodes_rsm_1=0
num_nodes_rsm_2=0
client=2
num_nodes_kafka=0
for v in ${rsm1_size[@]}; do
    if (( $v > $num_nodes_rsm_1 )); then num_nodes_rsm_1=$v; fi;
done
for v in ${rsm2_size[@]}; do
    if (( $v > $num_nodes_rsm_2 )); then num_nodes_rsm_2=$v; fi;
done

if [ "$kafka" = "true" ]; then num_nodes_kafka=4; fi;


echo "$num_nodes_rsm_1"
echo "$num_nodes_rsm_2"
# TODO Change to inputs!!
GP_NAME="exp-group"
TEMPLATE="kafka-unified-5-spot" # "kafka-unified-3-spot"

RSM1_ZONE="us-west4-a" # us-east1/2/3/4, us-south1, us-west1/2/3/4
RSM2_ZONE="us-west4-a"
KAFKA_ZONE="us-west4-a"

if [ "$run_dr" = "true" ] || [ "$run_ccf" = "true" ]; then
    RSM1_ZONE="us-west4-a" # us-east1/2/3/4, us-south1, us-west1/2/3/4
	RSM2_ZONE="us-east5-a"
	KAFKA_ZONE="us-east5-a"
fi

gcloud compute instances list --filter="name~^${GP_NAME}-rsm-1" --format='value(networkInterfaces[0].networkIP)' > /tmp/RSM1_ips.txt &
gcloud compute instances list --filter="name~^${GP_NAME}-rsm-2" --format='value(networkInterfaces[0].networkIP)' > /tmp/RSM2_ips.txt &
gcloud compute instances list --filter="name~^${GP_NAME}-kafka" --format='value(networkInterfaces[0].networkIP)' > /tmp/KAFKA_ips.txt &
wait

ar=($(cat /tmp/RSM1_ips.txt))
RSM1=(${ar[@]::${num_nodes_rsm_1}})
CLIENT=(${ar[@]:${num_nodes_rsm_1}}) # First client node is in RSM1
CLIENT_RSM1=(${ar[@]:${num_nodes_rsm_1}})

ar=($(cat /tmp/RSM2_ips.txt))
RSM2=(${ar[@]::${num_nodes_rsm_2}})
CLIENT+=(${ar[@]:${num_nodes_rsm_2}}) # Second client node would be in RSM2
CLIENT_RSM2=(${ar[@]:${num_nodes_rsm_2}})

ar=($(cat /tmp/KAFKA_ips.txt))
ZOOKEEPER=(${ar[@]::1})
KAFKA=(${ar[@]:1})

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

echo "Starting Experiment"

function makeExperimentJson() {
	r1size=$1
	r2size=$2
	r1fail=$3
	r2fail=$4
	pktsize=$5
	expName=$6
	isKafka=$7

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
	if [ ${isKafka} = "false" ]; then
		echo -e "    \"replication_protocol\": \"scrooge\"," >>experiments.json
	else
		echo -e "    \"replication_protocol\": \"kafka\"," >>experiments.json
	fi
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
if [ "${kafka}" = "true" ]; then
	protocols+=("kafka")
fi

raft_counter=0
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
	parallel -v --jobs=0 scp -o StrictHostKeyChecking=no -i "${key_file}" ${network_dir}{1} ${username}@{2}:"${exec_dir}" ::: network0urls.txt network1urls.txt ::: "${RSM1[@]:0:$r1_size}"
	parallel -v --jobs=0 scp -o StrictHostKeyChecking=no -i "${key_file}" ${network_dir}{1} ${username}@{2}:"${exec_dir}" ::: network0urls.txt network1urls.txt ::: "${RSM2[@]:0:$r2size}"


	############# Setup all necessary external applications #############
	joinedvar1=""
	joinedvar2=""
	raft_count=1
	function start_raft() {
		echo "Raft RSM is being used!"
		# Take in arguments
		local client_ips=("${!1}")
		local size=$2
		local RSM=("${!3}")
		local raft_pids=()
		local send_dr_txns=$4
		local send_ccf_txns=$5
		local single_replica_rsm=$6
		etcd_path="${raft_app_dir}etcd-main/"
		# Run setup build script
		#Client node
		for client_ip in "${client_ips[@]}"; do
			echo ${client_ip}
			ssh -i ${key_file} -o StrictHostKeyChecking=no -t "${client_ip}" 'cd '"${etcd_path}"' && export PATH=$PATH:/usr/local/go/bin &&  killall etcd; killall benchmark; git fetch && git reset --hard 047cfa5fa5d3d62cb200701224434979611bc3ab && chmod +x '"${etcd_path}"'scripts/build.sh && cd '"${etcd_path}"'; go install -v ./tools/benchmark' > /dev/null 2>&1 &
			raft_pids+=($!)
		done
		echo "Sent client build information!"

		for i in ${!RSM[@]}; do
			echo "building etcd on RSM: ${RSM[$i]}"
			ssh -i ${key_file} -o StrictHostKeyChecking=no ${RSM[$i]} "export PATH=\$PATH:/usr/local/go/bin; cd ${etcd_path}; killall etcd; killall benchmark; git fetch && git reset --hard 047cfa5fa5d3d62cb200701224434979611bc3ab; echo \$(pwd); chmod +x ./scripts/build.sh; ./scripts/build.sh" > /dev/null 2>&1 &
			raft_pids+=($!)
		done
		echo "Sent replica build information!"

		for pid in ${raft_pids[*]}; do
			wait $pid
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
			if [ "${single_replica_rsm}" = "false" ]; then
				(ssh -i ${key_file} -o StrictHostKeyChecking=no ${RSM[$i]} "export THIS_NAME=${this_name}; export THIS_IP=${this_ip}; export TOKEN=${TOKEN}; export CLUSTER_STATE=${CLUSTER_STATE}; export CLUSTER="${cluster_list%,}"; cd \$HOME; echo PWD: \$(pwd)  THIS_NAME:\${THIS_NAME} THIS_IP:\${THIS_IP} TOKEN:\${TOKEN} CLUSTER:\${CLUSTER}; killall -9 benchmark; sudo fuser -n tcp -k 2379 2380; sudo rm -rf \$HOME/data.etcd; echo \$HOME/.bashrc; ${etcd_bin_path}/etcd ${send_dr_txns} ${send_ccf_txns} --quota-backend-bytes=17179869184 --log-level error --data-dir=data.etcd --name \${THIS_NAME} --initial-advertise-peer-urls http://\${THIS_IP}:2380 --listen-peer-urls http://\${THIS_IP}:2380 --advertise-client-urls http://\${THIS_IP}:2379 --listen-client-urls http://\${THIS_IP}:2379 --initial-cluster \${CLUSTER} --initial-cluster-state \${CLUSTER_STATE} --initial-cluster-token \${TOKEN} --heartbeat-interval=100 --election-timeout=50000 &> etcd-log") > /dev/null 2>&1 &
			else
				(ssh -i ${key_file} -o StrictHostKeyChecking=no ${RSM[$i]} "export THIS_NAME=${this_name}; export THIS_IP=${this_ip}; export TOKEN=${TOKEN}; export CLUSTER_STATE=${CLUSTER_STATE}; export CLUSTER="${cluster_list%,}"; cd \$HOME; echo PWD: \$(pwd)  THIS_NAME:\${THIS_NAME} THIS_IP:\${THIS_IP} TOKEN:\${TOKEN} CLUSTER:\${CLUSTER}; killall -9 benchmark; sudo fuser -n tcp -k 2379 2380; sudo rm -rf \$HOME/data.etcd; echo \$HOME/.bashrc; ${etcd_bin_path}/etcd ${send_dr_txns} ${send_ccf_txns} --quota-backend-bytes=17179869184 --log-level error --data-dir=data.etcd --name \${THIS_NAME} --initial-advertise-peer-urls http://\${THIS_IP}:2380 --listen-peer-urls http://\${THIS_IP}:2380 --advertise-client-urls http://\${THIS_IP}:2379 --listen-client-urls http://\${THIS_IP}:2379 --initial-cluster-state \${CLUSTER_STATE} --initial-cluster-token \${TOKEN} --heartbeat-interval=100 --election-timeout=50000 &> etcd-log") > /dev/null 2>&1 &
			fi
			raft_counter=$((raft_counter + 1))
			if [ "${i}" -eq 0 ]; then
				sleep 5 # ensure node 0 gets elected
			fi

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
		local msg_size=$3
		local clients=0
		local connections=0
		shift 3
		local client_ips=("$@")

		if [ "${msg_size}" == "245" ]; then
			local clients=1200
			local connections=3
		elif [ "${msg_size}" == "498" ]; then
			local clients=1100
			local connections=3
		elif [ "${msg_size}" == "863" ]; then
			local clients=1100
			local connections=3
		elif [ "${msg_size}" == "1980" ]; then
			local clients=375
			local connections=3
		elif [ "${msg_size}" == "4020" ]; then
			local clients=140
			local connections=3
		elif [ "${msg_size}" == "8052" ]; then
			local clients=70
			local connections=3
		elif [ "${msg_size}" == "14304" ]; then
			local clients=40
			local connections=3
		fi

		echo "IN BENCHMARK_RAFT ${joinedvar}"
		for client_ip in "${client_ips[@]}"; do
			ssh -i ${key_file} -o StrictHostKeyChecking=no -t "${client_ip}" "source /home/scrooge/.bashrc; killall benchmark; /home/scrooge/go/bin/benchmark --dial-timeout=10000s --endpoints=\"${joinedvar}\" --conns=\"${connections}\" --clients=\"${clients}\" put --key-size=8 --key-space-size 1 --total=1000000000 --val-size=\"${msg_size}\"  1>benchmark_raft.log 2>&1" </dev/null &>/dev/null &
			pids_to_kill+=($!)
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
		parallel -v --jobs=0  -o StrictHostKeyChecking=no -i "${kescpy_file}" ${algorand_scripts_dir}/addresses/{1}_node.json ${username}@{1}:${algorand_app_dir}/wallet_app/node.json ::: "${RSM[@]:0:$((size))}";

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
	function start_kafka() {
		echo "Kafka is being used"!
		local size=$1 #number of brokers
		local zookeeper_ip=$2
        local num_partitions=$3
		local broker_ips=("${@:4}")
		echo "KAFKA LOG: ZOOKEEPER_IP=${zookeeper_ip}"
		echo "KAFKA LOG: BROKER_IPS=${broker_ips[@]}"

		for ip in "${broker_ips[@]}"; do
			ssh -o StrictHostKeyChecking=no -t "${ip}" "${kafka_dir}"/bin/kafka-server-stop.sh;
		done
		ssh -o StrictHostKeyChecking=no -t "${zookeeper_ip}" "${kafka_dir}"/bin/zookeeper-server-stop.sh;

		sleep 10

        # clean zookeeper log
        echo "KAFKA LOG: Cleaning Zookeeper logs!"
        # Setup kafka directory on each of the machines
        ssh -o StrictHostKeyChecking=no -t "${zookeeper_ip}" '
            rm -rf /tmp/zookeeper;
            nohup '"${kafka_dir}"'/bin/zookeeper-shell.sh localhost:2181 <<< "
                deleteall /brokers
                deleteall /admin
                deleteall /config
                quit" &'
        echo "KAFKA LOG: Zookeeper logs cleaned!"

		

		#set up zookeeper node
        echo "KAFKA LOG: Starting Zookeeper!"
        scp -o StrictHostKeyChecking=no ${kafka_dir}/config/zookeeper.properties "${zookeeper_ip}":${kafka_dir}/config
		ssh -f -o StrictHostKeyChecking=no -t "${zookeeper_ip}" 'source ~/.profile  && cd '"${kafka_dir}"' && nohup ./bin/zookeeper-server-start.sh ./config/zookeeper.properties >correct.log 2>error.log < /dev/null &'
        echo "KAFKA LOG: Zookeeper node successfully started!"

		#iterate and start each broker node
		count=0
		while ((count < size)); do
                # clean broker log
                echo "KAFKA LOG: Cleaning logs on Broker Node (${broker_ips[$count]})"
                ssh -o StrictHostKeyChecking=no -t "${broker_ips[$count]}" 'rm -rf /tmp/kafka-logs-0/'
				ssh -o StrictHostKeyChecking=no -t "${broker_ips[$count]}" 'rm /home/scrooge/kafka_2.13-3.7.0/logs/*'

                echo "KAFKA LOG: Logs cleaned for Broker Node (${broker_ips[$count]})"

				#create server.properties files
				echo "broker.id=${count}" > server.properties
				echo "listeners=PLAINTEXT://0.0.0.0:9092" >> server.properties
                echo "advertised.listeners=PLAINTEXT://${broker_ips[$count]}:9092" >> server.properties
				echo "log.dirs=/tmp/kafka-logs-0" >> server.properties
				echo "zookeeper.connect=${zookeeper_ip}:2181" >> server.properties
                echo "num.partitions=${num_partitions}" >> server.properties
				# echo "fetch.max.wait.ms=3000" >> server.properties            # Broker waits 3 seconds before sending data
				# echo "remote.fetch.max.wait.ms=3000" >> server.properties            # Broker waits 3 seconds before sending data
				# echo "replica.lag.time.max.ms=1000000" >> server.properties     # Tolerate lagging consumers up to 10 seconds
				# echo "replica.fetch.max.bytes=50000000" >>	server.properties
				# echo "replica.fetch.response.max.bytes=50000000" >> server.properties
				#scp server.properties to broker node
				scp -o StrictHostKeyChecking=no server.properties "${broker_ips[$count]}":${kafka_dir}/config
				ssh -o StrictHostKeyChecking=no -t "${broker_ips[$count]}" 'pkill -f scrooge-kafka; pkill -9 .*java.*'

				echo "KAFKA LOG: Starting Broker Node (${broker_ips[$count]})"
				ssh -f -o StrictHostKeyChecking=no -t "${broker_ips[$count]}" 'source ~/.profile && cd '"${kafka_dir}"' &&  nohup ./bin/kafka-server-start.sh ./config/server.properties >correct.log 2>error.log < /dev/null &' &>/dev/null </dev/null &
				#ssh -o StrictHostKeyChecking=no -t "${broker_ips[$count]}" 'source ~/.profile && (cd '"${kafka_dir}"' || exit) && exec -a scrooge-kafka ./bin/kafka-server-start.sh ./config/server.properties 1>/home/scrooge/kafka-broker-log 2>&1 &'
				count=$((count + 1))
		done
        echo "KAFKA LOG: All Broker Nodes successfully started!"
        broker_ips_string=$(printf "%s:9092," "${broker_ips[@]}")
		broker_ips_string="${broker_ips_string%,}" # removes trailing ,
		#start kafka from script node instead?
		sleep 20
		echo "KAFKA LOG: Creating Topic w/ Bootstrap Server ($broker_ips_string)"
		${kafka_dir}/bin/kafka-topics.sh --create --bootstrap-server $broker_ips_string --replication-factor $size --topic topic-1 --partitions $num_partitions
		${kafka_dir}/bin/kafka-topics.sh --create --bootstrap-server $broker_ips_string --replication-factor $size --topic topic-2 --partitions $num_partitions
		echo "KAFKA LOG: Topics successfully created!"
		#ssh -f -o StrictHostKeyChecking=no -t "${zookeeper_ip}" '(exec -a topic-1 '"${kafka_dir}"'/bin/kafka-topics.sh --create --bootstrap-server '"$broker_ips_string"' --replication-factor '"$size"' --topic topic-1 1>/home/scrooge/kafka-topic-log-1 2>&1 &) && (exec -a topic-2 '"${kafka_dir}"'/bin/kafka-topics.sh --create --bootstrap-server '"$broker_ips_string"' --replication-factor '"$size"' --topic topic-2 1>/home/scrooge/kafka-topic-log-2 2>&1 &)'
        echo "KAFKA LOG: All kafka nodes started successfully!"
	}

    function print_kafka_json() {
		local OUTPUT_FILENAME=$1
		local topic1=$2
		local topic2=$3
		local rsm_id=$4
		local node_id=$5
		local rsm_size=$6
		local read_from_pipe=$7
		local message=$8
		local benchmark_duration=${9}
		local warmup_duration=${10}
		local cooldown_duration=${11}
		local input_path=${12}
		local output_path=${13}
		local broker_ips=${14}
		local write_dr=${15}
		local write_ccf=${16}
		local message_size=${17}
		rm "$OUTPUT_FILENAME"
		echo "{" >> "$OUTPUT_FILENAME"
		echo "    \"topic1\": \"${topic1}\"," >> "$OUTPUT_FILENAME"
		echo "    \"topic2\": \"${topic2}\"," >> "$OUTPUT_FILENAME"
		echo "    \"rsm_id\": ${rsm_id}," >> "$OUTPUT_FILENAME"
		echo "    \"node_id\": ${node_id}," >> "$OUTPUT_FILENAME"
		echo "    \"broker_ips\": \"${broker_ips}\"," >> "$OUTPUT_FILENAME"
		echo "    \"rsm_size\": ${rsm_size}," >> "$OUTPUT_FILENAME"
		echo "    \"read_from_pipe\": ${read_from_pipe}," >> "$OUTPUT_FILENAME"
		echo "    \"message\": \"${message}\"," >> "$OUTPUT_FILENAME"
		echo "    \"benchmark_duration\": ${benchmark_duration}," >> "$OUTPUT_FILENAME"
		echo "    \"warmup_duration\": ${warmup_duration}," >> "$OUTPUT_FILENAME"
		echo "    \"cooldown_duration\": ${cooldown_duration}," >> "$OUTPUT_FILENAME"
		echo "    \"input_path\": \"${input_path}\"," >> "$OUTPUT_FILENAME"
		echo "    \"output_path\": \"${output_path}\"," >> "$OUTPUT_FILENAME"
		echo "    \"write_dr\": ${write_dr}," >> "$OUTPUT_FILENAME"
		echo "    \"write_ccf\": ${write_ccf}," >> "$OUTPUT_FILENAME"
		echo "    \"message_size\": ${message_size}" >> "$OUTPUT_FILENAME"
		echo "}" >> "$OUTPUT_FILENAME"
	}

  for algo in "${protocols[@]}"; do # Looping over all the protocols.
    scrooge="false"
    all_to_all="false"
    one_to_one="false"
    geobft="false"
    leader="false"
	kafka="false"

    if [ "${algo}" = "scrooge" ]; then
      scrooge="true"
    elif [ "${algo}" = "all_to_all" ]; then
      all_to_all="true"
    elif [ "${algo}" = "geobft" ]; then
       geobft="true"
    elif [ "${algo}" = "leader" ]; then
       leader="true"
	elif [ "${algo}" = "kafka" ]; then
		kafka="true"
    else
		one_to_one="true"
    fi

	for kl_size in "${klist_size[@]}"; do                    # Looping over all the klist_sizes.
	for pk_size in "${packet_size[@]}"; do                   # Looping over all the packet sizes.
	for bt_size in "${batch_size[@]}"; do                    # Looping over all the batch sizes.
	for bt_create_tm in "${batch_creation_time[@]}"; do      # Looping over all batch creation times.
	for pl_buf_size in "${pipeline_buffer_size[@]}"; do      # Looping over all pipeline buffer sizes.
	for noop_delay in "${noop_delays[@]}"; do                # etc...
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
			start_raft "CLIENT_RSM1[@]" "$r1_size" "RSM1[@]" "${run_dr}" "${run_ccf}" "false"
		elif [ "$send_rsm" = "file" ]; then
			echo "File RSM is being used for sending. No extra setup necessary."
		else
			echo "INVALID RECEIVING RSM."
		fi



		#if [kafka = true] then
			#compile executable
			#temporary clone in the executable, will change after adding kafka to this repository
			# git clone https://github.com/chawinphat/scrooge-kafka.git
			# cd scrooge-kafka
			# sbt package --> not working so we will clone repo into each node instead of scp'ing executable

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
			start_raft "CLIENT_RSM2[@]" "$r1_size" "RSM2[@]" "false" "${run_ccf}" "${run_dr}"
		elif [ "$receive_rsm" = "file" ]; then
			echo "File RSM is being used for receiving. No extra setup necessary."
		else
			echo "INVALID RECEIVING RSM."
		fi
		makeExperimentJson "${r1_size}" "${rsm2_size[$rcount]}" "${rsm1_fail[$rcount]}" "${rsm2_fail[$rcount]}" "${pk_size}" ${experiment_name} ${kafka}
		if [ $kafka = "true" ]; then
			echo "KAFKA LOG: Running Kafka Cluster - TODO ASSUMES RSM 2 size!"
			start_kafka 3 "${ZOOKEEPER[0]}" "${rsm2_size[$rcount]}" "${KAFKA[@]}"
			broker_ips_string=$(printf "%s:9092," "${KAFKA[@]}")
			broker_ips_string="${broker_ips_string%,}" # removes trailing ,

			# These files are created by running:
			# 1) `fallocate -l 1000 1kb_file.txt`
			# 2) `printf 'A%.0s' {1..1000} > 1kb_file.txt`
			file=""
			if [ "${pk_size}" = "100" ]; then
				file=$(<100b_file.txt)
			fi
			if [ "${pk_size}" = "1000" ]; then
				file=$(<1000b_file.txt)
			fi
			if [ "${pk_size}" = "10000" ]; then
				file=$(<10000b_file.txt)
			fi
			if [ "${pk_size}" = "100000" ]; then
				file=$(<100000b_file.txt)
			fi
			if [ "${pk_size}" = "1000000" ]; then
				file=$(<1000000b_file.txt)
			fi

			read_from_pipe="false"
			#if file_rsm is True, we don't want to read_from_pipe in our kafka config
			if [ "$file_rsm" = "false" ]; then
				read_from_pipe="true"
			fi

			echo "KAFKA LOG: Running RSM 1"
			for node in $(seq 0 $((rsm1_size - 1))); do
				print_kafka_json "config.json" "topic-1" "topic-2" "1" "${node}" "${r1_size}" "${read_from_pipe}" "${file}" "30" "20" "0" "/tmp/scrooge-input" "/tmp/scrooge-output" "${broker_ips_string}" "${run_dr}" "${run_ccf}" "${pk_size}"
				scp -o StrictHostKeyChecking=no config.json "${RSM1[$node]}":~/scrooge-kafka/src/main/resources/
			done

			echo "KAFKA LOG: Running RSM 2"
			for node in $(seq 0 $((rsm2_size - 1))); do
				print_kafka_json "config.json" "topic-1" "topic-2" "2" "${node}" "${r1_size}" "${read_from_pipe}" "${file}" "30" "20" "0" "/tmp/scrooge-input" "/tmp/scrooge-output" "${broker_ips_string}" "${run_dr}" "${run_ccf}" "${pk_size}"
				scp -o StrictHostKeyChecking=no config.json "${RSM2[$node]}":~/scrooge-kafka/src/main/resources/
			done

			echo "KAFKA LOG: Running experiment"
			./experiments/experiment_scripts/run_experiments.py ${workdir}/BFT-RSM/Code/experiments/experiment_json/experiments.json ${experiment_name} &
		    experiment_pid=$!
			if [ "$send_rsm" = "raft" ]; then
				echo "Running Send_RSM Benchmark Raft"
				benchmark_raft "${joinedvar1}" 1 "${pk_size}" "${CLIENT_RSM1[@]}"
			fi
			if [ "$receive_rsm" = "raft" ]; then
				if [ "$run_dr" = "false" ]; then
					echo "Running Receive_RSM Benchmark Raft"
					benchmark_raft "${joinedvar2}" 2 "${pk_size}" "${CLIENT_RSM2[@]}"
				fi
			fi

            wait $experiment_pid

			for pid in "${pids_to_kill[@]}"; do
				kill $pid
			done

			continue
		fi

		parallel -v --jobs=0 scp -oStrictHostKeyChecking=no -i "${key_file}" ${network_dir}{1} ${username}@{2}:"${exec_dir}" ::: network0urls.txt network1urls.txt ::: "${RSM1[@]:0:$r1_size}"
		parallel -v --jobs=0 scp -oStrictHostKeyChecking=no -i "${key_file}" ${network_dir}{1} ${username}@{2}:"${exec_dir}" ::: network0urls.txt network1urls.txt ::: "${RSM2[@]:0:$r2size}"


		# Next, we run the script.
		./experiments/experiment_scripts/run_experiments.py ${workdir}/BFT-RSM/Code/experiments/experiment_json/experiments.json ${experiment_name} &
		experiment_pid=$!

		sleep 1

		if [ "$send_rsm" = "raft" ]; then
			echo "Running Send_RSM Benchmark Raft"
			benchmark_raft "${joinedvar1}" 1 "${pk_size}" "${CLIENT_RSM1[@]}"
		fi
		if [ "$receive_rsm" = "raft" ]; then
			if [ "$run_dr" = "false" ]; then
				echo "Running Receive_RSM Benchmark Raft"
				benchmark_raft "${joinedvar2}" 2 "${pk_size}" "${CLIENT_RSM2[@]}"
			fi
		fi

		wait $experiment_pid

		for pid in "${pids_to_kill[@]}"; do
			kill $pid
		done

		pids_to_kill=()

		done;done;done;done;done;done;done;done;done;done

	rcount=$((rcount + 1))
done

for client_ip in "${CLIENT[@]}"; do
	ssh -i ${key_file} -o StrictHostKeyChecking=no -t "${client_ip}" 'killall benchmark' </dev/null &>/dev/null  &
done
