## Created By: Suyash Gupta - 08/23/2023
##
## This script helps to create the files that specify URLs for RSM1 and RSM2. Additionally, it calls the script that helps to create "config.h". We need to specify the URLs and stakes for each node in both the RSMs. This script takes in argument the size of both the RSMs and other necessary parameters.

#!/bin/bash

# State the IP addresses of the nodes that you want in RSM1.
RSM1=(
  "10.10.1.2"
  "10.10.1.3"
  "10.10.1.4"
  "10.10.1.5"
  "10.10.1.6"
  "10.10.1.7"
  "10.10.1.8"
  "10.10.1.2"
  "10.10.1.3"
  "10.10.1.4"
)

# State the IP addresses of the nodes that you want in RSM2.
RSM2=(
  "10.10.1.6"
  "10.10.1.7"
  "10.10.1.8"
  "10.10.1.9"
  "10.10.1.6"
  "10.10.1.7"
  "10.10.1.8"
  "10.10.1.9"
  "10.10.1.2"
  "10.10.1.3"
)

# Set rarely changing parameters.
warmup_time=20s
total_time=60s
num_packets=10000
network_dir="/proj/ove-PG0/suyash2/BFT-RSM/Code/configuration/" 
log_dir="/proj/ove-PG0/suyash2/BFT-RSM/Code/experiments/results/" 
json_dir="/proj/ove-PG0/suyash2/BFT-RSM/Code/experiments/experiment_json/"
experiment_name="scrooge_4_replica_stake"
batch_size=262144
batch_creation_time=4800us
max_nng_blocking_time=500ms
pipeline_buffer_size=2048 
message_buffer_size=256
klist_size=5120



#
# EXPERIMENT LIST
#
# State all experiments you want to run below. 
# State the stake of RSM1 and RSM2.
# Uncomment experiment you want to run.

## Exp: Equal stake RSMs of size 4; message size 100.
rsm1_size=(4) 
rsm2_size=(4)
rsm1_fail=(1)
rsm2_fail=(1)
RSM1_Stake=(1 1 1 1)
RSM2_Stake=(1 1 1 1)
packet_size=(100)
scrooge="true"
all_to_all="true"
one_to_one="true"
file_rsm="true"


### Exp: Equal stake RSMs of size 4; message size 100, 1000
#rsm1_size=(4) 
#rsm2_size=(4)
#rsm1_fail=(1)
#rsm2_fail=(1)
#RSM1_Stake=(1 1 1 1)
#RSM2_Stake=(1 1 1 1)
#packet_size=(100 1000)


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


makeExperimentJson()
{
  r1size=$1
  r2size=$2
  r1fail=$3
  r2fail=$4
  pktsize=$5
  expName=$6

  echo -e "{" > experiments.json
  echo -e "  \"experiment_independent_vars\": {" >> experiments.json
  echo -e "    \"project_dir\": \"/proj/ove-PG0/suyash2/BFT-RSM/Code/experiments/results/final_results/\"," >> experiments.json
  echo -e "    \"src_dir\": \"/proj/ove-PG0/suyash2/BFT-RSM/Code/\"," >> experiments.json
  echo -e "    \"network_dir\": \"${network_dir}\"," >> experiments.json
  echo -e "    \"local_setup_script\": \"/proj/ove-PG0/suyash2/BFT-RSM/Code/setup-seq.sh\"," >> experiments.json
  echo -e "    \"remote_setup_script\": \"/proj/ove-PG0/suyash2/BFT-RSM/Code/setup_remote.sh\"," >> experiments.json
  echo -e "    \"local_compile_script\": \"/proj/ove-PG0/suyash2/BFT-RSM/Code/build.sh\"," >> experiments.json
  echo -e "    \"replication_protocol\": \"scrooge\"," >> experiments.json

  echo -e "    \"clusterZeroIps\": [" >> experiments.json
  lcount=0
  while ((${lcount} < ${r1size})) 
  do
    	echo -e -n "      \"${RSM1[$lcount]}\"" >> experiments.json
	lcount=$((lcount+1))
	if [ ${lcount} -eq ${r1size} ]
	then
	  break
	fi
	echo -e "," >> experiments.json
  done
  echo "" >> experiments.json
  echo -e "    ]," >> experiments.json

  echo -e "    \"clusterOneIps\": [" >> experiments.json
  lcount=0
  while ((${lcount} < ${r2size})) 
  do
    	echo -e -n "      \"${RSM2[$lcount]}\"" >> experiments.json
	lcount=$((lcount+1))
	if [ ${lcount} -eq ${r2size} ]
	then
	  break
	fi
	echo -e "," >> experiments.json
  done
  echo "" >> experiments.json
  echo -e "    ]," >> experiments.json

  echo -e "    \"client_regions\": [" >> experiments.json
  echo -e "      \"US\"" >> experiments.json
  echo -e "    ]" >> experiments.json
  echo -e "  }," >> experiments.json

  echo -e "  \"${expName}\": {" >> experiments.json
  echo -e "    \"experiment_name\": \"increase_packet_size\"," >> experiments.json
  echo -e "    \"nb_rounds\": 1," >> experiments.json
  echo -e "    \"simulate_latency\": 0," >> experiments.json
  echo -e "    \"duration\": 4," >> experiments.json
  echo -e "    \"scrooge_args\": {" >> experiments.json
  echo -e "      \"general\": {" >> experiments.json
  echo -e "        \"use_debug_logs_bool\": 0," >> experiments.json
  echo -e "        \"log_path\": \"${log_dir}\"," >> experiments.json
  echo -e "        \"max_num_packets\": ${num_packets}" >> experiments.json
  echo -e "      }," >> experiments.json
  echo -e "      \"cluster_0\": {" >> experiments.json
  echo -e "        \"local_num_nodes\": [" >> experiments.json
  echo -e "          ${r1size}" >> experiments.json
  echo -e "        ]," >> experiments.json
  echo -e "        \"foreign_num_nodes\": [" >> experiments.json
  echo -e "          ${r2size}" >> experiments.json
  echo -e "        ]," >> experiments.json
  echo -e "        \"local_max_nodes_fail\": [" >> experiments.json
  echo -e "          ${r1fail}" >> experiments.json
  echo -e "        ]," >> experiments.json
  echo -e "        \"foreign_max_nodes_fail\": [" >> experiments.json
  echo -e "          ${r2fail}" >> experiments.json
  echo -e "        ]," >> experiments.json
  echo -e "        \"num_packets\": [" >> experiments.json
  echo -e "          ${num_packets}" >> experiments.json
  echo -e "        ]," >> experiments.json
  echo -e "        \"packet_size\": [" >> experiments.json
  echo -e "          ${pktsize}" >> experiments.json
  echo -e "        ]" >> experiments.json
  echo -e "      }," >> experiments.json
  echo -e "      \"cluster_1\": {" >> experiments.json
  echo -e "        \"local_num_nodes\": [" >> experiments.json
  echo -e "          ${r2size}" >> experiments.json
  echo -e "        ]," >> experiments.json
  echo -e "        \"foreign_num_nodes\": [" >> experiments.json
  echo -e "          ${r1size}" >> experiments.json
  echo -e "        ]," >> experiments.json
  echo -e "        \"local_max_nodes_fail\": [" >> experiments.json
  echo -e "          ${r2fail}" >> experiments.json
  echo -e "        ]," >> experiments.json
  echo -e "        \"foreign_max_nodes_fail\": [" >> experiments.json
  echo -e "          ${r1fail}" >> experiments.json
  echo -e "        ]," >> experiments.json
  echo -e "        \"num_packets\": [" >> experiments.json
  echo -e "          ${num_packets}" >> experiments.json
  echo -e "        ]," >> experiments.json
  echo -e "        \"packet_size\": [" >> experiments.json
  echo -e "          ${pktsize}" >> experiments.json
  echo -e "        ]" >> experiments.json
  echo -e "      }" >> experiments.json
  echo -e "    }" >> experiments.json
  echo -e "  }" >> experiments.json
  echo -e "}" >> experiments.json

  cp experiments.json ${json_dir} #copy to the expected folder.
}


#
# CREATING Config.h and network files.
#

rcount=0
protocols="scrooge alltoall onetoone"
for r1_size in ${rsm1_size[@]}
do
  for algo in ${protocols}
  do 
    scrooge="false"
    all_to_all="false"
    one_to_one="false"

    if [ "${algo}" = "scrooge" ]; then
      scrooge="true"
    elif [ "${algo}" = "alltoall" ]; then 
      all_to_all="true"
    else  
      one_to_one="true"
    fi  
		    
    for pk_size in ${packet_size[@]}
    do

      # First, we create the configuration file "network0urls.txt" through echoing and redirection.
      rm network0urls.txt  
      count=0
      while ((${count} < ${r1_size})) 
      do
      	echo -n ${RSM1[$count]} >> network0urls.txt
      	echo -n " " >> network0urls.txt
      	echo ${RSM1_Stake[$count]} >> network0urls.txt
      	count=$((count+1))
      done	
      cat network0urls.txt
      cp  network0urls.txt ${network_dir} #copy to the expected folder.
      echo " "

      # Next, we create the configuration file "network1urls.txt" through echoing and redirection.
      rm network1urls.txt  
      count=0
      while ((${count} < ${rsm2_size[$rcount]}))
      do
      	echo -n ${RSM2[$count]} >> network1urls.txt
      	echo -n " " >> network1urls.txt
      	echo ${RSM2_Stake[$count]} >> network1urls.txt
      	count=$((count+1))
      done
      cat network1urls.txt
      cp  network1urls.txt ${network_dir} #copy to the expected folder.
      echo " "

      # Next, we call the script that makes the config.h. We need to pass all the arguments.
      ./makeConfig.sh ${r1_size} ${rsm2_size[$rcount]} ${rsm1_fail[$rcount]} ${rsm2_fail[$rcount]} ${num_packets} ${pk_size} ${network_dir} ${log_dir} ${warmup_time} ${total_time} ${batch_size} ${batch_creation_time} ${max_nng_blocking_time} ${pipeline_buffer_size} ${message_buffer_size} ${klist_size} ${scrooge} ${all_to_all} ${one_to_one} ${file_rsm}

      cat config.h
      cp config.h system/

      # Next, we make the experiment.json for backward compatibility.
      makeExperimentJson ${r1_size} ${rsm2_size[$rcount]} ${rsm1_fail[$rcount]} ${rsm2_fail[$rcount]} ${pk_size} ${experiment_name} 

      # Next, we run the script.
      #./experiments/experiment_scripts/run_experiments.py /proj/ove-PG0/suyash2/BFT-RSM/Code/experiments/experiment_json/experiments.json ${experiment_name}

    done
  done  
  rcount=$((rcount+1))
done


	
