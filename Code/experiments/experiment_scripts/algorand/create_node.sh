#!/bin/bash

NODE=/proj/ove-PG0/therealmurray/node/dummy_node
NAME=test1

for i in 0 1
do
	for k in 0 # First for loop takes care of relay node
	do
		mkdir $NODE/r$i$k
		export ALGORAND_DATA=$NODE/r$i$k
		echo $ALGORAND_DATA
		~/go/bin/goal node generatetoken
		cp /proj/ove-PG0/therealmurray/node/genesis.json $ALGORAND_DATA/genesis.json
		cp /proj/ove-PG0/therealmurray/node/four_relay_config.json $ALGORAND_DATA/config.json
		mkdir $ALGORAND_DATA/privatenet-v1
	done
	for j in 1 2 3 4
	do
    	mkdir $NODE/n$i$j
		export ALGORAND_DATA=$NODE/n$i$j
		echo $ALGORAND_DATA
		~/go/bin/goal node generatetoken
		cp /proj/ove-PG0/therealmurray/node/genesis.json $ALGORAND_DATA/genesis.json
		cp /proj/ove-PG0/therealmurray/node/four_node_config.json $ALGORAND_DATA/config.json
		mkdir $ALGORAND_DATA/privatenet-v1
		expect <<-EOF
			proc abort {} {
				puts "Timeout or EOF\n";
				exit 1
			}
			set timeout 20
			spawn ~/go/bin/goal wallet new $NAME
			expect {
				"Please choose a password for wallet '$NAME':"          { send -- "\r" }
				default          abort
			}
			expect {
				"Please confirm the password:"          { send -- "\r" }
				default          abort
			}
			expect {
				"Would you like to see it now? (Y/n):"          { send -- "n" }
				default          abort
			}
		EOF
		~/go/bin/goal account new -w $NAME -f > $ALGORAND_DATA/address.txt
	done
done