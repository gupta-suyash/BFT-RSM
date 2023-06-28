#! /bin/bash

# Delete all snapshots and WAL stored in a raftexample instance
rm -rf /proj/ove-PG0/ethanxu/BFT-RSM/raftDG/etcd-main/contrib/raftexample/raftexample-*
rm /proj/ove-PG0/ethanxu/BFT-RSM/raftDG/etcd-main/contrib/raftexample/raftexample

echo "cleared snap!"