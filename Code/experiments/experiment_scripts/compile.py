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

clusterOne = ["10.10.1.2", "10.10.1.3", "10.10.1.4", "10.10.1.5", "10.10.1.11"]
clusterTwo = ["10.10.1.6", "10.10.1.7", "10.10.1.8", "10.10.1.9", "10.10.1.13"]

def main():
    if len(sys.argv) != 2:
        sys.stderr.write('Usage: python3 %s <bool for starting cluster 2>\n' % sys.argv[0])
        sys.exit(1)
    
    print("Argument: ", int(sys.argv[1]))
    cmd = ". /proj/ove-PG0/ethanxu/BFT-RSM/Code/experiments/experiment_scripts/compile.sh"
    hostname=socket.gethostname()   
    IPAddr=socket.gethostbyname(hostname)
    ssh_command_list = []
    for i in range(0, len(clusterOne)):
        cmd_ssh = "ssh -o StrictHostKeyChecking=no -t " + clusterOne[i] + " '" + cmd + "'"
        ssh_command_list.append(cmd_ssh)
    for i in range(0, len(clusterOne)):
        print("Host: ", clusterOne[i])
        hostname=socket.gethostname()
        IPAddr=socket.gethostbyname(hostname)
        if clusterOne[i] == IPAddr:
            print("Host is: ", IPAddr)
            continue
        subprocess.check_call(ssh_command_list[i], shell=True)

    # Start Cluster Two, if indicated via command line
    if int(sys.argv[1]):
        print("Cluster Two")
        ssh_command_list = []
        for i in range(0, len(clusterTwo)):
            cmd = ". /proj/ove-PG0/ethanxu/BFT-RSM/Code/experiments/experiment_scripts/compile.sh"
            cmd_ssh = "ssh -o StrictHostKeyChecking=no -t " + clusterTwo[i] + " '" + cmd + "'"
            ssh_command_list.append(cmd_ssh)
        for i in range(0, len(clusterTwo)):
            print("Host: ", clusterTwo[i])
            hostname=socket.gethostname()
            IPAddr=socket.gethostbyname(hostname)
            if clusterTwo[i] == IPAddr:
                print("Host is: ", IPAddr)
                continue
            subprocess.check_call(ssh_command_list[i], shell=True)
    # cmd = "/proj/ove-PG0/ethanxu/resdb/scrooge-resdb.sh"
    # executeCommand(cmd)
 
    # cmd = "/proj/ove-PG0/ethanxu/resdb/resdb-kill.sh"
    # executeCommand(cmd)
if __name__ == "__main__":
    main()
