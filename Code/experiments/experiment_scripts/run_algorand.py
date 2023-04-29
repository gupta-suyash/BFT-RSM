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

from experiment_fxns import *

hosts = ["10.10.1.12", "10.10.1.2", "10.10.1.3", "10.10.1.4", "10.10.1.5", "10.10.1.6", "10.10.1.7", "10.10.1.8", "10.10.1.9", "10.10.1.10" ]
# relay = ["10.10.1.11"]

def main():
    relay_cmd = ". /proj/ove-PG0/murray/BFT-RSM/Code/experiments/experiment_scripts/relay.sh"
    executeCommand(relay_cmd)


    hostname=socket.gethostname()   
    IPAddr=socket.gethostbyname(hostname) 
    print(hosts) 
    ssh_command_list = []
    thread_list = list()
    for i in range(0, len(hosts)):
        cmd = ". /proj/ove-PG0/murray/BFT-RSM/Code/experiments/experiment_scripts/participation.sh " + str(i)
        cmd_ssh = "ssh -o StrictHostKeyChecking=no -t " + hosts[i] + " '" + cmd + "'"
        ssh_command_list.append(cmd_ssh)
    for i in range(0, len(hosts)):
        print("Host: ", hosts[i])
        hostname=socket.gethostname()
        IPAddr=socket.gethostbyname(hostname)
        if hosts[i] == IPAddr:
            print("Host is: ", IPAddr)
            continue
        t = threading.Thread(target=executeCommand, args=(ssh_command_list[i],))
        thread_list.append(t)
    for t in thread_list:
        t.start()
    for t in thread_list:
        t.join()
if __name__ == "__main__":
    main()
