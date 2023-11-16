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
genesis="/scripts/default_genesis.json"
wallet_name="default"

def main():
    if len(sys.argv) < 5:
        sys.stderr.write('Usage: python3 %s <app_pathname> <script_pathname> <starting_algos> <config file> <client_ip>\n')
        sys.exit(1)
    app_pathname = sys.argv[1]
    script_pathname = sys.argv[2]
    # Step 1: Install relevant algorand software
    run_install = ". " + script_pathname + install_script + " " + app_pathname + " " + script_pathname + " " + wallet_name + " " + sys.argv[4]
    print("Run install: ", run_install)
    subprocess.check_call([". " + script_pathname + install_script, app_pathname, script_pathname, wallet_name, sys.argv[4]], shell=True, stdout=sys.stdout, stderr=sys.stdout)
    # Step 2: Setup Algorand nodes
    generate_partkey(script_pathname, setup_script, keygen_script,  int(sys.argv[3]), sys.argv[4])
    # At the end of this, address.txt + mini_genesis.json both created

def generate_partkey(pathname, keygen_script, starting_algos, client_ip):
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

    # Open local genesis json and write local account information
    hostname=socket.gethostname()
    IPAddr=socket.gethostbyname(hostname)
    genesisf = open("~/" + str(IPAddr) + "_gen.json", 'a') # get actual directory
    gen_entry = {'addr': addr, 'comment': 'test', "state": {"algo": starting_algos, "onl": 1, "sel": part_sel, "vote": part_vote, "voteKD": 10000, "voteLst": 3000000}}
    genesisf.write(json.dumps(gen_entry))
    

    # Open local address json and write local address information to facilitate wallet matching process
    addrf = open("~/" + str(IPAddr) + "_addr.json", 'a') # get actual directory
    node_path = pathname + "/node/"
    api_token = open(node_path + "/algod.token", 'r')
    kmd_token = open(node_path + "/kmd-v0.5/kmd.token", 'r')
    addr_entry =  {"api_token": api_token.readlines()[0].strip(), 
                    "kmd_token": kmd_token.readlines()[0].strip(), 
                    "send_acct": "<PLACEHOLDER>", 
                    "receive_acct": "<PLACEHOLDER>", 
                    "server_url": "http://127.0.0.1", 
                    "algo_port": 8080, 
                    "kmd_port": 7833, 
                    "wallet_port": 1234, 
                    "client_port": 4003, 
                    "algorand_port": 3456, 
                    "client_ip": client_ip,
                    "my_acct": addr
                  };
    addrf.write(json.dumps(addr_entry))
    
    # Close all open files
    partkeyf.close()
    genesisf.close()
    api_token.close()
    kmd_token.close()
    addrf.close()

if __name__ == "__main__":
    main()
