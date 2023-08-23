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

echo -e "#ifndef _CONFIG_H_" > config.h
echo -e "#define _CONFIG_H_" >> config.h
echo -e "#define OWN_RSM_SIZE ${own_rsm_size}" >> config.h
echo -e "#define OTHER_RSM_SIZE ${other_rsm_size}" >> config.h
echo -e "#define OWN_RSM_MAX_NODES_FAIL ${own_rsm_max_nodes_fail}" >> config.h
echo -e "#define OTHER_RSM_MAX_NODES_FAIL ${other_rsm_max_nodes_fail}" >> config.h
echo -e "#define NUMBER_PACKETS ${number_packets}" >> config.h
echo -e "#define PACKET_SIZE ${packet_size}" >> config.h

echo -e "#define NETWORK_DIR \"${network_dir}\"" >> config.h
echo -e "#define LOG_DIR \"${log_dir}"\" >> config.h
                              
echo -e "#define WARMUP_TIME ${warmup_time}" >> config.h
echo -e "#define TOTAL_TIME ${total_time}" >> config.h

echo -e "#define BATCH_SIZE ${batch_size}" >> config.h
echo -e "#define BATCH_CREATION_TIME ${batch_creation_time}" >> config.h
echo -e "#define MAX_NNG_BLOCKING_TIME ${max_nng_blocking_time}" >> config.h
echo -e "#define PIPELINE_BUFFER_SIZE ${pipeline_buffer_size}" >> config.h
echo -e "#define MESSAGE_BUFFER_SIZE ${message_buffer_size}" >> config.h
echo -e "#define KLIST_SIZE ${klist_size}" >> config.h

echo -e "#endif" >> config.h
