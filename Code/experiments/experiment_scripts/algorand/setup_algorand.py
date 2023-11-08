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

# setup_dir = os.path.realpath(os.path.dirname(__file__))
sys.path.append("../util/")
from json_util import *
from ssh_util import *

install_script="/scripts/install.sh"
setup_script="/scripts/setup.sh"
keygen_script="/scripts/keygen.sh"
node_config="/jsons/node_config.json"
relay_config="/jsons/relay_config.json"
genesis="/jsons/default_genesis.json"
wallet_name="default"

def main():
    if len(sys.argv) != 3:
        sys.stderr.write('Usage: python3 %s <app_pathname> <script_pathname> <starting_algos> <relay_bool> <main_server_ip>')
        sys.exit(1)
    app_pathname = sys.argv[1]
    script_pathname = sys.argv[2]
    # Step 1: Install relevant algorand software
    run_install = ". " + script_pathname + install_script + " " + app_pathname + " " + script_pathname + " " + wallet_name
    executeCommand(run_install)
    # Step 2: Setup Algorand nodes
    generate_partkey(script_pathname, setup_script, keygen_script,  int(sys.argv[3]))
    # At the end of this, address.txt + mini_genesis.json both created

def generate_partkey(pathname, keygen_script, starting_algos):
    # Default values
    sel_key = "sel"
    vote_key = "vote"
    subdir_path = pathname + "/node"
    # Get wallet address from account file
    accountLines = open(subdir_path + "/address.txt", 'r').readlines()
    addr = accountLines[0].strip().split(" ")[len(accountLines[0].strip().split()) - 1]
    # Generate partkeys
    partkeyinfo = ". " + pathname + keygen_script + " -p " + pathname + " -a " + addr
    executeCommand(partkeyinfo)

    # Read in Partkey json file and get sel + vote information
    partkeyf = open(subdir_path + "/partkeyinfo.json", 'r')
    partkeyLines = partkeyf.readlines()
    part_sel = partkeyLines[11].strip().split()[len(partkeyLines[11].strip().split()) - 1]
    part_vote = partkeyLines[12].strip().split()[len(partkeyLines[12].strip().split()) - 1]

    # Open genesis file and write in information and Add entry to genesis file here -- TODO CHANGE
    hostname=socket.gethostname()
    IPAddr=socket.gethostbyname(hostname)
    genesisf = open(subdir_path +  "/" + str(IPAddr) + ".json", 'r') # get actual directory
    genData = json.load(genesisf)
    entry = {'addr': addr, 'comment': 'test', "state": {"algo": starting_algos, "onl": 1, "sel": part_sel, "vote": part_vote, "voteKD": 10000, "voteLst": 3000000}}
    genData.append(entry)
    genesisWrites = open(subdir_path +  "/genesis.json", 'w')
    genesisWrites.write(json.dumps(genData))
    genesisWrites.close()
    partkeyf.close()
    genesisf.close()
    # NEED TO MAKE SURE MAIN SERVER KNOWS ALL THE ADDRESSES

if __name__ == "__main__":
    main()