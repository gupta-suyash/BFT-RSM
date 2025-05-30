## Created By: Suyash Gupta - 08/23/2023
##
## This script helps to create config.h file for Scrooge. It takes as input several parameters. Any new parameter that needs to be added to the config.h needs to be simply echoed to the file. We use redirection operator (>>) for echoing to the config.h. Each time running this script leads to overwriting existing config.h through the use of first redirection operator (>).


#!/bin/bash

own_rsm_size=$1
other_rsm_size=$2
own_rsm_max_nodes_fail=$3
other_rsm_max_nodes_fail=$4
number_packets=$5
packet_size=$6
network_dir=$7
log_dir=$8
warmup_time=$9
total_time=${10}
batch_size=${11}
batch_creation_time=${12}
max_nng_blocking_time=${13}
pipeline_buffer_size=${14}
message_buffer_size=${15}
klist_size=${16}
scrooge=${17}
all_to_all=${18}
one_to_one=${19}
geobft=${20}
leader=${21}
file_rsm=${22}
use_debug_logs_bool=${23}
noop_delay=${24}
max_message_delay=${25}
quack_window=${26}
ack_window=${27}
write_dr_txns=${28}
write_ccf_txns=${29}
byz_mode=${30}
simulate_crash=${31}
throttle_file=${32}


echo -e "#ifndef _CONFIG_H_" > config.h
echo -e "#define _CONFIG_H_" >> config.h
echo -e "#define OWN_RSM_SIZE ${own_rsm_size}" >> config.h
echo -e "#define OTHER_RSM_SIZE ${other_rsm_size}" >> config.h
echo -e "#define OWN_RSM_MAX_NODES_FAIL ${own_rsm_max_nodes_fail}" >> config.h
echo -e "#define OTHER_RSM_MAX_NODES_FAIL ${other_rsm_max_nodes_fail}" >> config.h
echo -e "#define NUMBER_PACKETS ${number_packets}" >> config.h
echo -e "#define PACKET_SIZE ${packet_size}" >> config.h

echo -e "#define NETWORK_DIR \"${network_dir}\"" >> config.h
echo -e "#define LOG_DIR \"${log_dir}\"" >> config.h
echo -e "#define USE_DEBUG_LOGS_BOOL ${use_debug_logs_bool}" >> config.h

echo -e "#define WARMUP_TIME ${warmup_time}" >> config.h
echo -e "#define TOTAL_TIME ${total_time}" >> config.h

echo -e "#define BATCH_SIZE ${batch_size}" >> config.h
echo -e "#define BATCH_CREATION_TIME ${batch_creation_time}" >> config.h
echo -e "#define MAX_NNG_BLOCKING_TIME ${max_nng_blocking_time}" >> config.h
echo -e "#define PIPELINE_BUFFER_SIZE ${pipeline_buffer_size}" >> config.h
echo -e "#define MESSAGE_BUFFER_SIZE ${message_buffer_size}" >> config.h
echo -e "#define KLIST_SIZE ${klist_size}" >> config.h
echo -e "#define SCROOGE ${scrooge}" >> config.h
echo -e "#define ALL_TO_ALL ${all_to_all}" >> config.h
echo -e "#define ONE_TO_ONE ${one_to_one}" >> config.h
echo -e "#define GEOBFT ${geobft}" >> config.h
echo -e "#define LEADER ${leader}" >> config.h
echo -e "#define FILE_RSM ${file_rsm}" >> config.h
echo -e "#define NOOP_DELAY ${noop_delay}" >> config.h
echo -e "#define MAX_MESSAGE_DELAY ${max_message_delay}" >> config.h
echo -e "#define QUACK_WINDOW ${quack_window}" >> config.h
echo -e "#define ACK_WINDOW ${ack_window}" >> config.h
echo -e "#define WRITE_DR ${write_dr_txns}" >> config.h
echo -e "#define WRITE_CCF ${write_ccf_txns}" >> config.h
echo -e "#define BYZ_MODE \"${byz_mode}\"" >> config.h
echo -e "#define SIMULATE_CRASH ${simulate_crash}" >> config.h
echo -e "#define THROTTLE_FILE ${throttle_file}" >> config.h

echo -e "#endif" >> config.h
echo -e "" >> config.h
