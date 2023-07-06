#! /bin/bash

put_kvip="10.10.1.9:12380"
leader_kvip="10.10.1.9:12380"

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

for ((c = 0; c < 5; c++)); do
    for ((i = 1; i <= num; i++)); do
        data=$((i + $c * $num))
        echo "writing (key-"$data", $data) to remote raft system"
        curl -L http://"$put_kvip"/key-"$data" -XPUT -d $data
        # curl -L http://"$leader_kvip"/key-"$i"
    done &
done

wait
