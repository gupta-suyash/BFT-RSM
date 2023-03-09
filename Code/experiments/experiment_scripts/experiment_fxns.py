# Natacha Crooks ncrooks@cs.utexas.edu 2017


# Main script for running experiments. This file is currently
# specific to Obladi code. It should be modified to run one's own
# custom experiment. However, the format and steps should be similar for
# all experiments.


# Each sript contains five parts:
# EC2 setup (optional)
# Setup - Sets up folders/binaries on all machines
# Run - Runs experiments
# Cleanup - Collects data, kills processes on remote machines
# EC2 teardown (optional)

# To add a new experiment, create a JSON deployment file (example: confic/tpcc/ec2.json)
# and a JSON experiment file (example: confic/tpcc/tpcc_10_oram.json)
# and create a python script with the following code:

# exampleExperiment.setupCloudlab("[path_to_setup_script]")
# exampleExperiment.setup("[experiment_name].json")
# exampleExperiment.run("[experiment_name].json")
# exampleExperiment.cleanup("test.json")
# exampleExperiment.cleanupEc2("test.json")

# TODO remove redundant pieces of code (some duplication). Move to an object datastructure

import os
import sys
import datetime
import time
import random
import multiprocessing
import subprocess

from dataclasses import dataclass
from typing import Optional, List, Union

# Include all utility scripts
setup_dir = os.path.realpath(os.path.dirname(__file__))
sys.path.append(setup_dir + "/util/")
from ssh_util import *
from json_util import *
from math_util import *
from graph_util import *

############ CONSTANTS
# Cloudlab Tags

@dataclass
class Scrooge_Args:
    use_debug_logs_bool: bool
    node_id: int
    local_network_urls: List[str]
    foreign_network_urls: List[str]
    local_max_nodes_fail: int
    foreign_max_nodes_fail: int
    num_packets: int
    packet_size: int
    log_path: str

@dataclass
class Example_Args:
    """
    Exists to make Union compile with one type
    """
    pass

@dataclass
class Setup_Args:
    wan_urls_to_setup: List[str]
    compile_script: str
    install_path: str

@dataclass
class Cloudlab_Experiment:
    experiment_args: Union[Scrooge_Args, Example_Args]
    setup_args: Optional[Setup_Args]

############

# Updates property file with the IP addresses obtained from setupEC2
# Sets up client machines and proxy machines as
# with ther (private) ip address.
# Each VM will be created with its role tag concatenated with
# the name of the experiment (ex: proxy-tpcc)
def setupServers(localSetupFile, remoteSetupFile, ip_list):
    subprocess.call(localSetupFile)
    executeSequenceBlockingRemoteCommand(ip_list, remoteSetupFile)

def compileCode(localCompileFile):
    subprocess.call(localCompileFile)

# Function that setups up appropriate folders on the
# correct machines, and sends the jars. It assumes
# that the appropriate VMs/machines have already started
def setup(configJsonPath, experimentName) -> List[Cloudlab_Experiment]:
    print("Starting Setup")

    config = loadJsonFile(configJsonPath)
    if not config:
        print("Empty config file, failing")
        return []

    ##### LOADING CONFIG FILE ####
    # Username for ssh-ing.
    # Name of the experiment that will be run
    exp_independent_vars = config['experiment_independent_vars']
    cloudlab.experiment_name = experimentName
    # Experiment results dir on the machine
    cloudlab.project_dir = config['experiment_independent_vars']['project_dir']
    # Source directory on the local machine (for compilation)
    cloudlab.src_dir = config['experiment_independent_vars']['src_dir']
    # Path to setup script
    cloudlab.local_setup_script = exp_independent_vars['local_setup_script']
    # Path to setup script for remote machines
    cloudlab.remote_setup_script = exp_independent_vars['remote_setup_script']
    # Compile the program once on the local machine
    compileCode(exp_independent_vars['local_compile_script'])
    # The date is used to distinguish multiple runs of the same experiment
    expFolder = cloudlab.project_dir + cloudlab.experiment_name
    expDir =  expFolder + "/" + datetime.datetime.now().strftime("%Y:%m:%d:%H:%M") + "/"
    config['experiment_independent_vars']["experiments_dir"] = expDir

    #### GENERATING EXP DIRECTORY ON ALL MACHINES ####
    print("Creating Experiment directory")
    executeCommand("mkdir -p " + expDir)

    # Create file with git hash
    executeCommand("cp " + configJsonPath + " " + cloudlab.project_dir)
    gitHash = getGitHash(cloudlab.src_dir)
    print("Saving Git Hash " + str(gitHash))
    executeCommand("touch " + expDir + "/git.txt")
    with open(expDir + "/git.txt", 'ab') as f:
        f.write(gitHash)
    return cloudlab

# Runs the actual experiment
def run(configJsonPath, experimentName):
    # Load local arguments
    cloudlab = Cloudlab_Experiment()
    config = loadJsonFile(configJsonPath)
    # Name of the experiment that will be run
    cloudlab.experiment_name = experimentName
    # Experiment results dir on the machine
    cloudlab.project_dir = config['experiment_independent_vars']['project_dir']
    # Source directory on the local machine (for compilation)
    cloudlab.src_dir = config['experiment_independent_vars']['src_dir']

    # The numclients field is a list that contains a list of client counts.
    # Ex, if this is listed: [1,2,4,8], the framework will run the experiment
    # 4 times: one with 1 clients, then with two, then four, then 8. The
    # format for collecting the data will be remoteExpDir/clientcount.
    # If true, simulate latency with tc
    try:
        simulateLatency = int(config[experimentName]['simulate_latency'])
    except:
        simulateLatency = 0

    # Setup latency on appropriate hosts if simulated
    if (simulateLatency):
        print("Simulating a " + str(simulateLatency) + " ms")

    first = True
    dataLoaded = False
    increase_packet_size = Scaling_Client_Exp()
    increase_packet_size.num_rounds = int(config[experimentName]['num_rounds'])
    # Run for each round, num_rounds time.
    
    for round in range(0, increase_packet_size.num_rounds):
        time.sleep(10)
        try:
            # Need to collect the scrooge start commands
            scrooge_commands
            cluster_zero_size = int(config[experimentName]['scrooge_args']['cluster_0']['local_num_nodes'][round])
            cluster_one_size = int(config[experimentName]['scrooge_args']['cluster_1']['local_num_nodes'][round])
            cluster_one_nodes = config['experiment_independent_vars'][]
            scrooge_exec = "/proj/ove-PG0/murray/Scrooge/Code/scrooge "
            groupId = 0
            nodeId = 0
            for j in range(0, clusterZerosz + clusterOnesz):
                cmd = scrooge_exec + configJsonPath + " " + experimentName + " " + str(groupId) + " " + str(nodeId) + " " + str(i)
                nodeId += 1
                if nodeId == clusterZerosz:
                    nodeId = 0
                    groupId = 1
                scrooge_commands.append(cmd)
            print(scrooge_commands)
            print("Execute command now")
            executeParallelBlockingDifferentRemoteCommands(ip_list, scrooge_commands)
        except Exception as e:
            print("Error in execution on round", round, "Exception: ", e)
            return
