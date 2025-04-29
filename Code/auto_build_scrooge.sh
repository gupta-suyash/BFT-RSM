#!/bin/bash

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

# Valid options :  "INF" "ZERO" "DELAY" "NO"

if [ "$protocol" = "KAFKA" ]; then
    # Don't compile anything for kafka
	exit 0
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


./makeConfig.sh "${rsm1_size[0]}" "${rsm2_size[0]}" "${rsm1_fail[0]}" "${rsm2_fail[0]}" ${num_packets} "${packet_size[0]}" ${network_dir} ${log_dir} ${warmup_time} ${total_time} "${batch_size[0]}" "${batch_creation_time[0]}" ${max_nng_blocking_time} "${pipeline_buffer_size[0]}" ${message_buffer_size} "${klist_size[0]}" ${scrooge} ${all_to_all} ${one_to_one} ${geobft} ${leader} ${file_rsm} ${use_debug_logs_bool} ${noop_delays[0]} ${max_message_delays[0]} ${quack_windows[0]} ${ack_windows[0]} ${run_dr} ${run_ccf} ${byz_mode} ${simulate_crash} ${throttle_file}

cp config.h system/

make clean
make proto
make -j </dev/null 1>/dev/null 2>&1

