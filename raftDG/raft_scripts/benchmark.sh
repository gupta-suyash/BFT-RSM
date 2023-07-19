#! /bin/bash

HOST_1="10.10.1.1:2379"
HOST_2="10.10.1.2:2379"
HOST_3="10.10.1.3:2379"
HOST_4="10.10.1.4:2379"

HOST_5="10.10.1.5:2379"
HOST_6="10.10.1.6:2379"
HOST_7="10.10.1.7:2379"
HOST_8="10.10.1.8:2379"

# ENDPOINTS=$HOST_1
# etcdctl --endpoints=$ENDPOINTS put foo "Hello World!"

# benchmark --endpoints=${HOST_1},${HOST_2},${HOST_3},${HOST_4} --conns=100 --clients=1000 put --key-size=8 --sequential-keys --total=3000000 --val-size=256 &

# benchmark --endpoints=${HOST_5},${HOST_6},${HOST_7},${HOST_8} --conns=100 --clients=1000 put --key-size=8 --sequential-keys --total=3000000 --val-size=256 &



(benchmark --endpoints=${HOST_1},${HOST_2},${HOST_3},${HOST_4} --conns=100 --clients=1000 put --key-size=8 --sequential-keys --total=1500000 --val-size=256 
benchmark --endpoints=${HOST_1},${HOST_2},${HOST_3},${HOST_4} --conns=100 --clients=1000 put --key-size=8 --sequential-keys --total=1500000 --val-size=256
benchmark --endpoints=${HOST_1},${HOST_2},${HOST_3},${HOST_4} --conns=100 --clients=1000 put --key-size=8 --sequential-keys --total=2000000 --val-size=256
benchmark --endpoints=${HOST_1},${HOST_2},${HOST_3},${HOST_4} --conns=100 --clients=1000 put --key-size=8 --sequential-keys --total=1000000 --val-size=256) &

(benchmark --endpoints=${HOST_5},${HOST_6},${HOST_7},${HOST_8} --conns=100 --clients=1000 put --key-size=8 --sequential-keys --total=1500000 --val-size=256
benchmark --endpoints=${HOST_5},${HOST_6},${HOST_7},${HOST_8} --conns=100 --clients=1000 put --key-size=8 --sequential-keys --total=1500000 --val-size=256
benchmark --endpoints=${HOST_5},${HOST_6},${HOST_7},${HOST_8} --conns=100 --clients=1000 put --key-size=8 --sequential-keys --total=2000000 --val-size=256
benchmark --endpoints=${HOST_5},${HOST_6},${HOST_7},${HOST_8} --conns=100 --clients=1000 put --key-size=8 --sequential-keys --total=1000000 --val-size=256) &
wait
