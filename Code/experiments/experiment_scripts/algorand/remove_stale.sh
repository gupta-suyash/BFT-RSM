#!/bin/bash

NODE=/proj/ove-PG0/therealmurray/node/dummy_node

for i in 0 1
do
    for j in 1 2 3 4
    do
        rm $NODE/n$i$j/privatenet-v1/crash.*
        rm $NODE/n$i$j/privatenet-v1/ledger.*
    done
done