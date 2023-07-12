#! /bin/bash
HOST_1="10.10.1.1:2379"
HOST_2="10.10.1.2:2379"
HOST_3="10.10.1.3:2379"

benchmark --endpoints=${HOST_1} --target-leader --conns=1 --clients=1 put --key-size=8 --sequential-keys --total=10000 --val-size=256

# benchmark --endpoints=${HOST_1},${HOST_2},${HOST_3} --conns=100 --clients=1000 put --key-size=8 --sequential-keys --total=100000 --val-size=256