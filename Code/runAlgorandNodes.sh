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

#If this experiment is for File_RSM (not algo or resdb)
file_rsm="true"
if [ "$send_rsm" != "file" ] || [ "$receive_rsm" != "file" ]; then
    file_rsm="false"
fi


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

if [ "$file_rsm" = "false" ]; then
	echo "WARNING: FILE RSM NOT BEING USED"
fi

echo "The applications you are running are $send_rsm and $receive_rsm." 
echo -n "Please acknolwedge and accept/reject. Type Y or N: "

read application_acknowledgement

if [ $application_acknowledgement != "Y" ]; then
	echo "Please check the application you are running."
	echo "Otherwise you will be sad :') Exiting now..."
	exit 1
fi

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
# TODO Change to inputs!!
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


	# Setup all necessary external applications
	function start_algorand() {
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
    echo "About to start applications!"
    start_algorand "${CLIENT[0]}" "$r1_size" "RSM1[@]"
	start_algorand "${CLIENT[1]}" "$r1_size" "RSM2[@]"
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
                            echo "WARNING !!!!!THIS IS WRONG YOU NEED THE IF STATEMENT IN THE FOR LOOP!!!!!!! WARNING"
							./makeConfig.sh "${r1_size}" "${rsm2_size[$rcount]}" "${rsm1_fail[$rcount]}" "${rsm2_fail[$rcount]}" ${num_packets} "${pk_size}" ${network_dir} ${log_dir} ${warmup_time} ${total_time} "${bt_size}" "${bt_create_tm}" ${max_nng_blocking_time} "${pl_buf_size}" ${message_buffer_size} "${kl_size}" ${scrooge} ${all_to_all} ${one_to_one} ${file_rsm} ${use_debug_logs_bool}

							cat config.h
							cp config.h system/

							make clean
							make proto
							make -j scrooge

							# Next, we make the experiment.json for backward compatibility.
							makeExperimentJson "${r1_size}" "${rsm2_size[$rcount]}" "${rsm1_fail[$rcount]}" "${rsm2_fail[$rcount]}" "${pk_size}" ${experiment_name}
							parallel -v --jobs=0 scp -oStrictHostKeyChecking=no -i "${key_file}" ${network_dir}{1} ${username}@{2}:"${exec_dir}" ::: network0urls.txt network1urls.txt ::: "${RSM1[@]:0:$r1_size}"
							parallel -v --jobs=0 scp -oStrictHostKeyChecking=no -i "${key_file}" ${network_dir}{1} ${username}@{2}:"${exec_dir}" ::: network0urls.txt network1urls.txt ::: "${RSM2[@]:0:$r2size}"

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
