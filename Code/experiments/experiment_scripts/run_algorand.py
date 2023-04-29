#!/usr/bin/env python3
import sys
import os
import subprocess
import threading
import socket
import concurrent.futures

setup_dir = os.path.realpath(os.path.dirname(__file__))
sys.path.append(setup_dir + "/util/")
from ssh_util import *

from experiment_fxns import *

participation_list = ["10.10.1.5", "10.10.1.4", "10.10.1.3", "10.10.1.2", "10.10.1.9", "10.10.1.8", "10.10.1.7", "10.10.1.6", "10.10.1.10", "10.10.1.12"]
# relay = ["10.10.1.11"]

def main():
    relay_cmd = "source ./relay.sh"
    subprocess.call(relay_cmd)


    hostname=socket.gethostname()   
    IPAddr=socket.gethostbyname(hostname)  
    for i in range(0, len(participation_list)):
        cmd = "source ./participation.sh " + i
        if participation_list[i] == IPAddr:
            print("Host is: ", IPAddr)
            continue
        cmd = "ssh -o StrictHostKeyChecking=no -t " + participation_list[i] + " '" + cmd + "'"
        subprocess.check_call(cmd, shell=True)
    run()

if __name__ == "__main__":
    main()