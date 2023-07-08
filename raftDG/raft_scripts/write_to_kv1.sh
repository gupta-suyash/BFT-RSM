#! /bin/bash

# put_kvip="10.10.1.1:11380"
# leader_kvip="10.10.1.1:11380"

kv_ips=(
  "10.10.1.1:11380" 
  "10.10.1.2:12380" 
  "10.10.1.3:13380"
  "10.10.1.4:14380" 
)

# take number from command line argument, and write from (key-1, 1) to (key-$num, $num) 
# to remote raft system
num=$1

# Check if an argument is provided
if [ -z "$num" ]; then
    echo "Please provide a number as an argument."
    exit 1
fi

# Check if argument is a valid integer
if ! [[ "$num" =~ ^[0-9]+$ ]]; then
    echo "Invalid argument. Please provide a valid integer."
    exit 1
fi

for n in "${!kv_ips[@]}"; do
    kv_ip="${kv_ips[$n]}"

    # for each node, spawn 6 clients to send kv requests in parallel
    for ((c = 0; c < 6; c++)); do
        for ((i = 1; i <= num; i++)); do
            data=$((i + c * num))
            echo "writing (key-"$data", $data) to remote raft system"
            curl -L http://"$kv_ip"/key-"$data" -XPUT -d $data
            # curl -L http://"$leader_kvip"/key-"$i"
        done &
    done

done

wait
