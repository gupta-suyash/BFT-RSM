#! /usr/bin/env python3
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

sys.path.append("/home/scrooge/BFT-RSM/Code/experiments/experiment_scripts/util/")
from json_util import *
from ssh_util import *

run_script="/scripts/run.sh"
wallet_name="default"

def main():
    if len(sys.argv) < 3:
        sys.stderr.write('Usage: python3 %s <addr_json_path> <current_server_ip> <prev_server_ip> <next_server_ip>')
        sys.exit(1)
    update_accts(sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4])

# Update local wallet app json
def update_accts(path_to_json, curr_ip, send_ip, receive_ip): # Check the client IP
    currf = open(path_to_json + "/" + curr_ip + "_addr.json", 'r+') # get actual directory
    sendf = open(path_to_json + "/" + send_ip + "_addr.json", 'r+')
    recvf = open(path_to_json + "/" + receive_ip + "_addr.json", 'r+')
    nodef = open(path_to_json + "/" + receive_ip + "_node.json", 'a')
    currdata = json.load(currf)
    senddata = json.load(sendf)
    recvdata = json.load(recvf)
    currdata["send_acct"] = senddata["my_acct"]
    currdata["receive_acct"] = recvdata["my_acct"]
    nodef.write(json.dumps(currdata))
    currf.close()
    sendf.close()
    recvf.close()
    nodef.close()

if __name__ == "__main__":
    main()
