#!/usr/bin/env python3
import sys
import os
import subprocess
import threading
import socket
import datetime
import time
import random
import multiprocessing
import concurrent.futures
import json

setup_dir = os.path.realpath(os.path.dirname(__file__))
sys.path.append("/proj/ove-PG0/therealmurray/BFT-RSM/Code/experiments/experiment_scripts/util/")
from ssh_util import *
# clusterOne = ["10.10.1.10", "10.10.1.11"]
clusterOne = ["10.10.1.10", "10.10.1.2", "10.10.1.3", "10.10.1.4", "10.10.1.5"] # first entry is the relay
clusterTwo = ["10.10.1.11", "10.10.1.6", "10.10.1.7", "10.10.1.8", "10.10.1.9"] # first entry is the relay
pathname = "/proj/ove-PG0/therealmurray/node/dummy_node/"
def main():
    if len(sys.argv) != 3:
        sys.stderr.write('Usage: python3 %s <number of nodes> <wallet name>\n' % sys.argv[0])
    setup_cluster(int(sys.argv[1]), sys.argv[2])
    # if len(sys.argv) != 3:
    #     sys.stderr.write('Usage: python3 %s <bool for starting cluster 2> <type of command we use>\n' % sys.argv[0])
    #     sys.exit(1)
    # print("Argument: ", int(sys.argv[1]), int(sys.argv[2]))
    # base_cmd = ""
    # if int(sys.argv[2]) == 0: # Setup all nodes
    #     base_cmd = ". /proj/ove-PG0/therealmurray/BFT-RSM/Code/experiments/experiment_scripts/algorand/compile.sh "
    # elif int(sys.argv[2]) == 1: # Run all nodes
    #     base_cmd = ". /proj/ove-PG0/therealmurray/BFT-RSM/Code/experiments/experiment_scripts/algorand/participation_rsm.sh "
    # else: # Shutdown all nodes
    #     base_cmd = ". /proj/ove-PG0/therealmurray/BFT-RSM/Code/experiments/experiment_scripts/algorand/shutdown.sh "

    # hostname=socket.gethostname()   
    # IPAddr=socket.gethostbyname(hostname)
    # ssh_command_list = []
    # thread_list = list()
    # for i in range(0, len(clusterOne)):
    #     cmd = base_cmd + str(i) + " 0 " + clusterOne[0] + ":4161"
    #     cmd_ssh = "ssh -o StrictHostKeyChecking=no -t " + clusterOne[i] + " '" + cmd + "'"
    #     print("Host: ", clusterOne[i])
    #     hostname=socket.gethostname()
    #     IPAddr=socket.gethostbyname(hostname)
    #     if clusterOne[i] == IPAddr:
    #         print("Host is: ", IPAddr)
    #         continue
    #     if int(sys.argv[2]) == 0:
    #             executeCommand(cmd_ssh)
    #     else:
    #         t = threading.Thread(target=executeCommand, args=(cmd_ssh,))
    #         thread_list.append(t)
    # if int(sys.argv[1]):
    #     print("Cluster Two")
    #     ssh_command_list = []
    #     for i in range(0, len(clusterTwo)):
    #         cmd = base_cmd + str(i) + " 1 " + clusterTwo[0] + ":4161"
    #         cmd_ssh = "ssh -o StrictHostKeyChecking=no -t " + clusterTwo[i] + " '" + cmd + "'"
    #         print("Host: ", clusterTwo[i])
    #         hostname=socket.gethostname()
    #         IPAddr=socket.gethostbyname(hostname)
    #         if clusterTwo[i] == IPAddr:
    #             print("Host is: ", IPAddr)
    #             continue
    #         if int(sys.argv[2]) == 0:
    #             executeCommand(cmd_ssh)
    #         else:
    #             t = threading.Thread(target=executeCommand, args=(cmd_ssh,))
    #             thread_list.append(t)
    # for t in thread_list:
    #     t.start()

def setup_cluster(num_nodes, wallet_name):
    # Keys
    sel_key = "sel"
    vote_key = "vote"

    hostname=socket.gethostname()   
    IPAddr=socket.gethostbyname(hostname)
    generate_cmd = ". " + pathname + "generate.sh"
    executeCommand(generate_cmd) # Generates directories
    clusters = clusterOne[1:] + clusterTwo[1:]
    relay = [clusterOne[0], clusterTwo[0]]
    for i in range(0, len(relay)):
        # Setup relay nodes here
    for i in range(0, 2):
        for j in range(1, num_nodes + 1):
            if clusters[i] == IPAddr:
                print("Host is: ", IPAddr)
                continue
            create_wallet = pathname + "spawnscript.sh"
            cmd_ssh_wallet = "ssh -o StrictHostKeyChecking=no -t " + clusters[i] + " '" + create_wallet + "'"
            executeCommand(cmd_ssh_wallet)
            addr_path = pathname + "address.txt"
            create_account = "~/go/bin/goal account new -w test1 -f > " + addr_path # parse output by spaces, then get address as the last element, check length
            cmd_ssh_account = "ssh -o StrictHostKeyChecking=no -t " + clusters[i] + " '" + create_account + "'"
            executeCommand(cmd_ssh_account)

            # Read in account file
            accountFile = open(addr_path, 'r')
            accountLines = accountFile.readlines()
            addr = accountLines[0].strip().split(" ")[len(accountLines[0].strip().split()) - 1]

            # Add partkeys
            add_partkeys = "ssh -o StrictHostKeyChecking=no -t " + clusters[i] + " '" + "~/go/bin/goal account addpartkey -a " + addr + "--roundFirstValid=1 --roundLastValid=6000000  --keyDilution=10000"  + "'"
            executeCommand(add_partkeys)
            partkey_path = pathname + "partkeyinfo.json"
            partkeyinfo = "ssh -o StrictHostKeyChecking=no -t " + clusters[i] + " '" + "~/go/bin/goal account partkeyinfo > " + partkey_path  + "'"
            executeCommand(partkeyinfo)

            # Read in Partkey json file
            partkeyf = open(partkey_path)
            partData = json.load(partkeyf)

            # Open genesis file and write in information and Add entry to genesis file here
            genesis_path = "/proj/ove-PG0/therealmurray/node/four_node/n" +  str(i) /*NOT FINAL*/ +  " /partkeyinfo.json"
            genesisf = open('') # get actual directory
            genData = json.load(genesisf)
            entry = {
                "addr": addr,
                "comment": "test1",
                "state": {
                    "algo": 1000000000000000,
                    "onl": 1,
                    "sel": partData[sel_key],
                    "vote": partData[vote_key],
                    "voteKD": 10000,
                    "voteLst": 3000000
                }
            }
            entry_json = json.dumps(entry, indent=4)
            genesisf.seek(0)
            genData["alloc"] = entry_json
            genesisf.write(json.dumps(genData))
            partkeyf.close()
            genesisf.close()

            # After all this, remove the privatenet information
            rm_stale_data = "rm ~node/testnetdata/privatenet-v1/crash.*; rm ~node/testnetdata/privatenet-v1/ledger.*"
            executeCommand(rm_stale_data)

            # And theoretically...algorand should be ready to run! Run using participation_rsm.sh
    

if __name__ == "__main__":
    main()