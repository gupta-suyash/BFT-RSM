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

# Updates property file with the IP addresses obtained from setupEC2
# Sets up client machines and proxy machines as
# with ther (private) ip address.
# Each VM will be created with its role tag concatenated with
# the name of the experiment (ex: proxy-tpcc)
def setupServers(localSetupFile, remoteSetupFile, ip_list):
    subprocess.call(localSetupFile)
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
    local_setup = config['experiment_independent_vars']['local_setup_script']
    # Path to setup script for remote machines
    remote_setup = config['experiment_independent_vars']['remote_setup_script']
    # List of IPs for every machine in the cluster
    ip_list = get_ips()
    print(ip_list)
    # Run function to install all appropriate packages on servers
    subprocess.call(localSetupFile)
    executeSequenceBlockingRemoteCommand(ip_list, remoteSetupFile)
