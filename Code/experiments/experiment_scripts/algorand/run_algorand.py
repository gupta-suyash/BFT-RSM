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

sys.path.append("../util/")
from json_util import *
from ssh_util import *

run_script="/scripts/run.sh"
wallet_name="default"

def main():
    update_wallets()
    executeCommand(pathname + "/run.sh")

# Update local wallet app json
def update_wallets(app_pathname, send_acct, receive_acct): # Check the client IP
    node_path = app_pathname + "/node/"
    wallet_path = app_pathname + "/go-algorand/wallet_app/node.json"
    appFile = open(wallet_path, 'w')
    api_token = open(node_path + "/algod.token", 'r')
    kmd_token = open(node_path + "/kmd-v0.5/kmd.token", 'r')
    appDict = {"api_token": api_token.readlines()[0].strip(), 
               "kmd_token": kmd_token.readlines()[0].strip(), 
               "send_acct": send_acct, 
               "receive_acct": receive_acct, 
               "server_url": "http://127.0.0.1", 
               "algo_port": 8080, 
               "kmd_port": 7833, 
               "wallet_port": 1234, 
               "client_port": 4003, 
               "algorand_port": 3456, 
               "client_ip": "128.110.218.203"
               };
    appFile.write(json.dumps(appDict))
    appFile.close()

if __name__ == "__main__":
    main()