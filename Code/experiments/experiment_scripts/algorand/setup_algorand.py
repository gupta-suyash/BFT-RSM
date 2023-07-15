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
sys.path.append(setup_dir + "/util/")
from ssh_util import *

clusterOne = ["10.10.1.2", "10.10.1.3", "10.10.1.4", "10.10.1.5"]
clusterTwo = ["10.10.1.6", "10.10.1.7", "10.10.1.8", "10.10.1.9"]
relay = ["10.10.1.11"]
relay2 = ["10.10.1.13"]

# clusterOne = ["10.10.1.2", "10.10.1.3", "10.10.1.4", "10.10.1.5", "10.10.1.10", "10.10.1.6", "10.10.1.7"]
# clusterTwo = []
# relay = ["10.10.1.8"]
# relay2 = []

def main():
    if len(sys.argv) != 2:
        sys.stderr.write('Usage: python3 %s <bool for starting cluster 2>\n' % sys.argv[0])
        sys.exit(1)
    
    print("Argument: ", int(sys.argv[1]))
    # Start Cluster 1 - always do!
    relay_cmd = ". /proj/ove-PG0/murray/BFT-RSM/Code/experiments/experiment_scripts/relay.sh"
    cmd_ssh = "ssh -o StrictHostKeyChecking=no -t " + relay[0] + " '" + relay_cmd + "'"
    executeCommand(cmd_ssh)

    if int(sys.argv[1]):
        relay2_cmd = ". /proj/ove-PG0/murray/BFT-RSM/Code/experiments/experiment_scripts/relay2.sh"
        cmd_ssh = "ssh -o StrictHostKeyChecking=no -t " + relay2[0] + " '" + relay2_cmd + "'"
        print("Cluster 2 execute command!")
        executeCommand(cmd_ssh)

    hostname=socket.gethostname()   
    IPAddr=socket.gethostbyname(hostname)
    ssh_command_list = []
    thread_list = list()
    for i in range(0, len(clusterOne)):
        cmd = ". /proj/ove-PG0/murray/BFT-RSM/Code/experiments/experiment_scripts/participation_rsm1.sh " + str(i + 1)
        cmd_ssh = "ssh -o StrictHostKeyChecking=no -t " + clusterOne[i] + " '" + cmd + "'"
        ssh_command_list.append(cmd_ssh)
    for i in range(0, len(clusterOne)):
        print("Host: ", clusterOne[i])
        hostname=socket.gethostname()
        IPAddr=socket.gethostbyname(hostname)
        if clusterOne[i] == IPAddr:
            print("Host is: ", IPAddr)
            continue
        t = threading.Thread(target=executeCommand, args=(ssh_command_list[i],))
        thread_list.append(t)

    # Start Cluster Two, if indicated via command line
    if int(sys.argv[1]):
        print("Cluster Two")
        ssh_command_list = []
        for i in range(0, len(clusterTwo)):
            cmd = ". /proj/ove-PG0/murray/BFT-RSM/Code/experiments/experiment_scripts/participation_rsm2.sh " + str(i+6)
            cmd_ssh = "ssh -o StrictHostKeyChecking=no -t " + clusterTwo[i] + " '" + cmd + "'"
            ssh_command_list.append(cmd_ssh)
        for i in range(0, len(clusterTwo)):
            print("Host: ", clusterTwo[i])
            hostname=socket.gethostname()
            IPAddr=socket.gethostbyname(hostname)
            if clusterTwo[i] == IPAddr:
                print("Host is: ", IPAddr)
                continue
            t = threading.Thread(target=executeCommand, args=(ssh_command_list[i],))
            thread_list.append(t)
    # cd_cmd = "cd /proj/ove-PG0/murray/BFT-RSM/Code/"
    # cmd = "./experiments/experiment_scripts/run_experiments.py /proj/ove-PG0/murray/BFT-RSM/Code/experiments/experiment_json/experiments.json increase_packet_size_nb_one > myout.txt"
    # executeCommand(cd_cmd)
    # executeCommand(cmd)
    for t in thread_list:
        t.start()
    

    # Start scrooge here if not running with resdb:
    for t in thread_list:
        t.join()

if __name__ == "__main__":
    main()