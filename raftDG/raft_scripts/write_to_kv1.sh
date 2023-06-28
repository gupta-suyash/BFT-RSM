#! /bin/bash

# put_kvip="128.110.218.101:13380"
# leader_kvip="128.110.218.105:11380"

# put_kvip="128.110.218.104:18380"
# leader_kvip="128.110.218.89:15380"

put_kvip="10.10.1.1:11380"
leader_kvip="10.10.1.1:12380"

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

for ((i = 1; i <= num; i++)); do
    echo "\nwriting (key-"$i", $i) to remote raft system"
    curl -L http://"$put_kvip"/key-"$i" -XPUT -d $i
    # curl -L http://"$leader_kvip"/key-"$i"
done