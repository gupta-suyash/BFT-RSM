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
clusterOne = ["10.10.1.6", "10.10.1.2", "10.10.1.3", "10.10.1.4", "10.10.1.5"] # first entry is the relay
clusterTwo = ["10.10.1.18", "10.10.1.14", "10.10.1.15", "10.10.1.16", "10.10.1.17"] # first entry is the relay
pathname = "/proj/ove-PG0/therealmurray/node/dummy_node/"
def main():
    if len(sys.argv) != 5:
        sys.stderr.write('Usage: python3 %s <bool for starting cluster 2> <type of command we use> <number of nodes> <wallet name> \n' % sys.argv[0])
        sys.exit(1)
    print("Arguments: ", int(sys.argv[1]), int(sys.argv[2]), int(sys.argv[3]), sys.argv[4]) # 1 2 4 test1
    base_cmd = ""
    if int(sys.argv[2]) == 0: # Setup all nodes
        base_cmd = ". /proj/ove-PG0/therealmurray/BFT-RSM/Code/experiments/experiment_scripts/algorand/compile.sh "
    elif int(sys.argv[2]) == 1: # Setup clusters
        setup_cluster(int(sys.argv[3]), sys.argv[4])
        return
    elif int(sys.argv[2]) == 2: # Run all nodes
        base_cmd = ". /proj/ove-PG0/therealmurray/BFT-RSM/Code/experiments/experiment_scripts/algorand/participation_rsm.sh "
    else: # Shutdown all nodes
        base_cmd = ". /proj/ove-PG0/therealmurray/BFT-RSM/Code/experiments/experiment_scripts/algorand/shutdown.sh "

    hostname=socket.gethostname()   
    IPAddr=socket.gethostbyname(hostname)

    thread_list = list()
    for i in range(0, len(clusterOne)):
        cmd = base_cmd + str(i) + " 0 " + clusterOne[0] + ":4161"
        cmd_ssh = "ssh -o StrictHostKeyChecking=no -t " + clusterOne[i] + " '" + cmd + "'"
        print("Host: ", clusterOne[i])
        hostname=socket.gethostname()
        IPAddr=socket.gethostbyname(hostname)
        if clusterOne[i] == IPAddr:
            print("Host is: ", IPAddr)
            continue
        if int(sys.argv[2]) == 0:
                executeCommand(cmd_ssh)
        else:
            t = threading.Thread(target=executeCommand, args=(cmd_ssh,))
            thread_list.append(t)
    if int(sys.argv[1]):
        print("Cluster Two")
        ssh_command_list = []
        for i in range(0, len(clusterTwo)):
            cmd = base_cmd + str(i) + " 1 " + clusterTwo[0] + ":4161"
            cmd_ssh = "ssh -o StrictHostKeyChecking=no -t " + clusterTwo[i] + " '" + cmd + "'"
            print("Host: ", clusterTwo[i])
            hostname=socket.gethostname()
            IPAddr=socket.gethostbyname(hostname)
            if clusterTwo[i] == IPAddr:
                print("Host is: ", IPAddr)
                continue
            if int(sys.argv[2]) == 0:
                executeCommand(cmd_ssh)
            else:
                t = threading.Thread(target=executeCommand, args=(cmd_ssh,))
                thread_list.append(t)
    for t in thread_list:
        t.start()

def setup_cluster(num_nodes, wallet_name):
    # Keys
    sel_key = "sel"
    vote_key = "vote"
    starting_algos = 10000000000000000
    print("Initializing algorand network with ", num_nodes, " and wallet name ", wallet_name)
    generate_cmd = pathname + "create_node.sh"
    executeCommand(generate_cmd) # Generates directories
    address_arr = []
    for i in range(0, 2):
        address_arr.append([])
        for j in range(1, num_nodes + 1):
            subdir_path = pathname + "n" + str(i) + str(j)
            # Read in account file
            addr_path = subdir_path + "/address.txt"
            accountFile = open(addr_path, 'r')
            accountLines = accountFile.readlines()
            addr = accountLines[0].strip().split(" ")[len(accountLines[0].strip().split()) - 1]
            genesis_addr = addr
            address_arr[i].append(addr)
            # continue # REMOVE
            # # print(addr)
            # # Add partkeys
            partkeyinfo = pathname + "keygen.sh -i " + str(i) + " -j " + str(j) + " -a " + addr
            executeCommand(partkeyinfo)

            # ----- UP TO HERE
            # Read in Partkey json file
            partkey_path = subdir_path + "/partkeyinfo.json"
            partkeyf = open(partkey_path, 'r')
            partkeyLines = partkeyf.readlines()
            # print(partkeyLines)
            part_sel = partkeyLines[11].strip().split()[len(partkeyLines[11].strip().split()) - 1]
            part_vote = partkeyLines[12].strip().split()[len(partkeyLines[12].strip().split()) - 1]
            # print(part_sel)
            # print(part_vote)

    #         # Open genesis file and write in information and Add entry to genesis file here
            genesis_path = subdir_path +  "/genesis.json"
            genesisf = open(genesis_path, 'r') # get actual directory
            genData = json.load(genesisf)
            entry = {'addr': genesis_addr, 'comment': 'test', "state": {"algo": int(starting_algos/num_nodes), "onl": 1, "sel": part_sel, "vote": part_vote, "voteKD": 10000, "voteLst": 3000000}}
            # genesisf.seek(0)
            genData["alloc"].append(entry)
            # print(genData)
            genesisWrites = open(genesis_path, 'w')
            # print(json.dumps(genData))
            genesisWrites.write(json.dumps(genData))
            genesisWrites.close()
            partkeyf.close()
            genesisf.close()
            print("Made it here!")

    for i in range(0, 2):
        for j in range(1, num_nodes+1):
            subdir_path = pathname + "n" + str(i) + str(j)
            # Update wallet app jsons
            app_path = "/proj/ove-PG0/therealmurray/go-algorand/wallet_app/node/node" + str(i) + str(j) + ".json"
            appFile = open(app_path, 'w')
            # print(j)
            api_token = open(subdir_path + "/algod.token", 'r')
            kmd_token = open(subdir_path + "/kmd-v0.5/kmd.token", 'r')
            idx = (j) % (num_nodes)
            print(idx)
            if idx == 0:
                idx = 1
            # print(address_arr[i][idx])
            appDict = {"api_token": api_token.readlines()[0].strip(), "kmd_token": kmd_token.readlines()[0].strip(), "send_acct": address_arr[i][j-1], "receive_acct": address_arr[i][idx], "server_url": "http://127.0.0.1", "algo_port": 8080, "kmd_port": 7833, "wallet_port": 1234, "client_port": 4003, "algorand_port": 3456, "client_ip": "128.110.218.203"}
            print(json.dumps(appDict))
            appFile.write(json.dumps(appDict))
            appFile.close()

    # After all this, remove the privatenet information - UPDATE COMMAND
    rm_stale_data = pathname + "remove_stale.sh"
    executeCommand(rm_stale_data)

    # And theoretically...algorand should be ready to run! Run using participation_rsm.sh
    

if __name__ == "__main__":
    main()