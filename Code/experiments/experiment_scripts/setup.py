#!/usr/bin/env python3
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
setup_dir = os.path.realpath(os.path.dirname(__file__))
sys.path.append(setup_dir + "/util/")
from ssh_util import *
from json_util import *

def main():
    if len(sys.argv) != 2:
        sys.stderr.write(f'Usage: python3 {sys.argv[0]} <config_file>\n')
        sys.exit(1)
    print(f'Arg: {sys.argv[1]}')
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
    ip_list = config['experiment_independent_vars']['clusterZeroIps'] + config['experiment_independent_vars']['clusterOneIps']
    print(ip_list)
    # Run function to install all appropriate packages on servers
    subprocess.call(localSetupFile)
    executeSequenceBlockingRemoteCommand(ip_list, remoteSetupFile)

if __name__ == "__main__":
    main()
