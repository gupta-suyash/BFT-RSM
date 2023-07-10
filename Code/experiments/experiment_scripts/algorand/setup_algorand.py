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

setup_dir = os.path.realpath(os.path.dirname(__file__))
sys.path.append("/proj/ove-PG0/therealmurray/BFT-RSM/Code/experiments/experiment_scripts/util/")
from ssh_util import *
# clusterOne = ["10.10.1.10", "10.10.1.11"]
clusterOne = ["10.10.1.10", "10.10.1.2", "10.10.1.3", "10.10.1.4", "10.10.1.5"] # first entry is the relay
clusterTwo = ["10.10.1.11", "10.10.1.6", "10.10.1.7", "10.10.1.8", "10.10.1.9"] # first entry is the relay

def main():
    if len(sys.argv) != 3:
        sys.stderr.write('Usage: python3 %s <bool for starting cluster 2> <type of command we use>\n' % sys.argv[0])
        sys.exit(1)
    print("Argument: ", int(sys.argv[1]), int(sys.argv[2]))
    base_cmd = ""
    if int(sys.argv[2]) == 0: # Setup all nodes
        base_cmd = ". /proj/ove-PG0/therealmurray/BFT-RSM/Code/experiments/experiment_scripts/algorand/compile.sh "
    elif int(sys.argv[2]) == 1: # Run all nodes
        base_cmd = ". /proj/ove-PG0/therealmurray/BFT-RSM/Code/experiments/experiment_scripts/algorand/participation_rsm.sh "
    else: # Shutdown all nodes
        base_cmd = ". /proj/ove-PG0/therealmurray/BFT-RSM/Code/experiments/experiment_scripts/algorand/shutdown.sh "

    hostname=socket.gethostname()   
    IPAddr=socket.gethostbyname(hostname)
    ssh_command_list = []
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

if __name__ == "__main__":
    main()