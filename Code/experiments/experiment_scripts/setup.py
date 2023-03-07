# Written: Natacha Crooks ncrooks@cs.utexas.edu 2017
# Edited: Micah Murray m_murray@berekely.edu 2023

# Setup - Sets up folders/binaries on all machines
import os
import os.path
import sys
import datetime
import time
import random
import multiprocessing
import subprocess

# Include all utility scripts
sys.path.append("/proj/ove-PG0/murray/Scrooge/Code/experiments/experiment_scripts/util/")
from ssh_util import *
from json_util import *

def main():
    if len(sys.argv) != 2:
        sys.stderr.write('Usage: python3 %s <config_file>\n' % sys.argv[0])
        sys.exit(1)
    print("Arg: %s", sys.argv[1])
    setup(sys.argv[1])

# Updates property file with the IP addresses obtained from setupEC2
# Sets up client machines and proxy machines as
# with ther (private) ip address.
# Each VM will be created with its role tag concatenated with
# the name of the experiment (ex: proxy-tpcc)
def setupServers(localSetupFile, remoteSetupFile, ip_list):
    #subprocess.call(localSetupFile)
    executeSequenceBlockingRemoteCommand(ip_list, remoteSetupFile)

# Function that setups up appropriate folders on the
# correct machines, and sends the jars. It assumes
# that the appropriate VMs/machines have already started
def setup(configJson):
    print("Setup")
    config = loadJsonFile(configJson)
    if not config:
        print("Empty config file, failing")
        return
    # Path to setup script
    localSetupFile = config['experiment_independent_vars']['local_setup_script']
    # Path to setup script for remote machines
    remoteSetupFile = config['experiment_independent_vars']['remote_setup_script']
    # List of IPs for every machine in the cluster
    ip_list = get_ips()
    print(ip_list)
    # Run function to install all appropriate packages on servers
    subprocess.call(localSetupFile)
    executeSequenceBlockingRemoteCommand(ip_list, remoteSetupFile)

def get_ips():
    host_file = open('/etc/hosts', 'r')
    ip_list = []
    Lines = host_file.readlines()
    for line in Lines:
        arr = line.split(" ")
        ip = arr[0].split('\t')
        ip_list.append(ip[0])
    return ["10.10.1.2"]

if __name__ == "__main__":
    main()
