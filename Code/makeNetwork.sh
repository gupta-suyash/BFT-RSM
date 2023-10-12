## Created By: Suyash Gupta - 08/23/2023
##
## This script helps to create the files that specify URLs for RSM1 and RSM2. Additionally, it calls the script that helps to create "config.h". We need to specify the URLs and stakes for each node in both the RSMs. This script takes in argument the size of both the RSMs and other necessary parameters.

#!/bin/bash

# State the IP addresses of the nodes that you want in RSM1.
RSM1=(
  "10.10.1.4"
  #"10.10.1.1"
  #"10.10.1.4"
  #"10.10.1.5"
)

# State the IP addresses of the nodes that you want in RSM2.
RSM2=(
  "10.10.1.3"
  #"10.10.1.4"
  #"10.10.1.8"
  #"10.10.1.9"
)

# Name of profile we are running out of
profile="therealmurray" # TODO: Replace with your directory name
key_file="~/.ssh/cloudlab_key" # TODO: Replace with your ssh key
username="murray22" # TODO: Replace with your username

# Set rarely changing parameters.
warmup_time=10s
total_time=30s
num_packets=10000
exec_dir="~/"
network_dir="/proj/ove-PG0/${profile}/BFT-RSM/Code/configuration/" 
log_dir="/proj/ove-PG0/${profile}/BFT-RSM/Code/experiments/results/" 
json_dir="/proj/ove-PG0/${profile}/BFT-RSM/Code/experiments/experiment_json/"
experiment_name="scrooge_4_replica_stake"
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
scrooge="true"
all_to_all="true"
one_to_one="true"
file_rsm="true"  #If this experiment is for File_RSM (not algo or resdb)

### DUMMY Exp: Equal stake RSMs of size 4; message size 100.
rsm1_size=(1) 
rsm2_size=(1)
rsm1_fail=(0)
rsm2_fail=(0)
RSM1_Stake=(1 1 1 1)
RSM2_Stake=(1 1 1 1)
klist_size=(64)
packet_size=(100)
batch_size=(26214)
batch_creation_time=(1ms)
pipeline_buffer_size=(8)

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
  echo -e "    \"project_dir\": \"/proj/ove-PG0/${profile}/BFT-RSM/Code/experiments/results/final_results/\"," >> experiments.json
  echo -e "    \"src_dir\": \"/proj/ove-PG0/${profile}/BFT-RSM/Code/\"," >> experiments.json
  echo -e "    \"network_dir\": \"${network_dir}\"," >> experiments.json
  echo -e "    \"exec_dir\": \"${exec_dir}\"," >> experiments.json
  echo -e "    \"ssh_key\": \"${key_file}\"," >> experiments.json
  echo -e "    \"username\": \"${username}\"," >> experiments.json
  echo -e "    \"local_setup_script\": \"/proj/ove-PG0/${profile}//BFT-RSM/Code/setup-seq.sh\"," >> experiments.json
  echo -e "    \"remote_setup_script\": \"/proj/ove-PG0/${profile}/BFT-RSM/Code/setup_remote.sh\"," >> experiments.json
  echo -e "    \"local_compile_script\": \"/proj/ove-PG0/${profile}/BFT-RSM/Code/build.sh\"," >> experiments.json
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

for r1_size in ${rsm1_size[@]} # Looping over all the network sizes
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

  # scp network files to expected directory on other machines
  count=0
  while ((${count} < ${rsm1_size[$rcount]}))
  do
	  scp -oStrictHostKeyChecking=no -i ${key_file} ${network_dir}/network0urls.txt ${username}@${RSM1[$count]}:${exec_dir}
	  scp -oStrictHostKeyChecking=no -i ${key_file} ${network_dir}/network1urls.txt ${username}@${RSM1[$count]}:${exec_dir}
	  # echo "scp -oStrictHostKeyChecking=no -i ${key_file} ${network_dir}/network0urls.txt ${RSM1[$count]}:${exec_dir}"
	  count=$((count+1))
  done
  count=0
  while ((${count} < ${rsm2_size[$rcount]}))
  do
	  scp -oStrictHostKeyChecking=no -i ${key_file} ${network_dir}/network1urls.txt ${username}@${RSM2[$count]}:${exec_dir}
	  scp -oStrictHostKeyChecking=no -i ${key_file} ${network_dir}/network0urls.txt ${username}@${RSM2[$count]}:${exec_dir}
	  # echo "scp -oStrictHostKeyChecking=no -i ${key_file} ${network_dir}/network1urls.txt ${username}@${RSM2[$count]}:${exec_dir}"
	  count=$((count+1))
  done

  for algo in ${protocols[@]} # Looping over all the protocols.
  do 
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
    for kl_size in ${klist_size[@]} # Looping over all the klist_sizes.
    do	    
      for pk_size in ${packet_size[@]} # Looping over all the packet sizes.
      do
        for bt_size in ${batch_size[@]} # Looping over all the batch sizes.
        do
          for bt_create_tm in ${batch_creation_time[@]} # Looping over all batch creation times.
          do
            for pl_buf_size in ${pipeline_buffer_size[@]} # Looping over all pipeline buffer sizes.
            do
        	    # Next, we call the script that makes the config.h. We need to pass all the arguments.
        	    ./makeConfig.sh ${r1_size} ${rsm2_size[$rcount]} ${rsm1_fail[$rcount]} ${rsm2_fail[$rcount]} ${num_packets} ${pk_size} ${network_dir} ${log_dir} ${warmup_time} ${total_time} ${bt_size} ${bt_create_tm} ${max_nng_blocking_time} ${pl_buf_size} ${message_buffer_size} ${kl_size} ${scrooge} ${all_to_all} ${one_to_one} ${file_rsm} ${use_debug_logs_bool}

        	    cat config.h
        	    cp config.h system/

                    make clean
                    make proto
                    make -j scrooge

        	    # Next, we make the experiment.json for backward compatibility.
        	    makeExperimentJson ${r1_size} ${rsm2_size[$rcount]} ${rsm1_fail[$rcount]} ${rsm2_fail[$rcount]} ${pk_size} ${experiment_name} 

        	    # Next, we run the script.
        	    ./experiments/experiment_scripts/run_experiments.py /proj/ove-PG0/${profile}/BFT-RSM/Code/experiments/experiment_json/experiments.json ${experiment_name}

            done
          done  
        done
      done
    done  
  done  
  rcount=$((rcount+1))
done


	
