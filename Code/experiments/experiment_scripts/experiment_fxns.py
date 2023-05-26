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

# Include all utility scripts
setup_dir = os.path.realpath(os.path.dirname(__file__))
sys.path.append(setup_dir + "/util/")
from ssh_util import *
from json_util import *
from math_util import *
from graph_util import *

############ CONSTANTS
# Cloudlab Tags
class Cloudlab_Experiment:
    experiment_name = ""
    experiment_dir = ""
    src_dir = ""
    setup_script = ""
    ip_list = []

class Scrooge_Args:
    use_debug_logs_bool = 0
    node_id = 0
    group_id = 0
    local_num_nodes = 0
    foreign_num_nodes = 0
    local_max_nodes_fail = 0
    foreign_max_nodes_fail = 0
    own_network_id = 0
    num_packets = 0
    packet_size = 0
    log_path = 0

class Scaling_Client_Exp:
    num_replicas = 0
    scaling_factor = 0
    nb_rounds = 0
    nb_groups = 0
    num_replicas_per_group = 0
    num_byzantine_replicas = 0
    simulate_latency = 0
# Number of times the experiment will be run (TODO: cleanup)
nbRepetitions = 3
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
def setup(configJson, experimentName):
    print("Setup")
    cloudlab = Cloudlab_Experiment()
    config = loadJsonFile(configJson)
    if not config:
        print("Empty config file, failing")
        return
##### LOADING CONFIG FILE ####
    # Username for ssh-ing.
    # Name of the experiment that will be run
    cloudlab.experiment_name = experimentName
    # Experiment results dir on the machine
    cloudlab.project_dir = config['experiment_independent_vars']['project_dir']
    # Source directory on the local machine (for compilation)
    cloudlab.src_dir = config['experiment_independent_vars']['src_dir']
    # Path to setup script
    cloudlab.local_setup_script = config['experiment_independent_vars']['local_setup_script']
    # Path to setup script for remote machines
    cloudlab.remote_setup_script = config['experiment_independent_vars']['remote_setup_script']
    # Compile the program once on the local machine
    compileCode(config['experiment_independent_vars']['local_compile_script'])
    # The date is used to distinguish multiple runs of the same experiment
    expFolder = cloudlab.project_dir + cloudlab.experiment_name
    expDir =  expFolder + "/" + datetime.datetime.now().strftime("%Y:%m:%d:%H:%M") + "/"
    config['experiment_independent_vars']["experiments_dir"] = expDir

#### GENERATING EXP DIRECTORY ON ALL MACHINES ####
    print("Creating Experiment directory")
    executeCommand("mkdir -p " + expDir)

    # Create file with git hash
    executeCommand("cp " + configJson + " " + os.path.join(expDir, "experiment_json.txt"))
    gitHash = getGitHash(cloudlab.src_dir)
    print("Saving Git Hash " + str(gitHash))
    executeCommand("touch " + os.path.join(expDir, "git.txt"))
    with open(expDir + "/git.txt", 'ab') as f:
        f.write(gitHash.encode())
    return expDir

# Generate Network files
def generateNetwork(networkConfigDir, cluster0sz, cluster1sz):
    host_file = open('/etc/hosts', 'r')
    Lines = host_file.readlines()
    count = -1
    clusterZero = []
    clusterOne = []
    hostDict = {}
    ip_list = []
    for line in Lines:
        count += 1
        if count == 0:
            continue
        arr = line.split(" ")
        ip = arr[0].split('\t')
        print(arr)
        node_idx_arr = arr[len(arr) - 1].strip().split('node')
        print(node_idx_arr)
        hostDict[int(node_idx_arr[len(node_idx_arr) - 1])] = ip[0]
        ip_list.append(ip[0])
    print("Dictionary ", hostDict)
    # TODO Figure out partition of hosts in the network 
    # for hosts in host
    offset = 0
    for i in range(0, 2):
        filename = "network" + str(i) + "urls.txt"
        executeCommand("rm " + networkConfigDir + filename)
        executeCommand("touch " + networkConfigDir + filename);
        with open(networkConfigDir + filename, 'w') as f:
            if i == 0:
                sz = cluster0sz
            else:
                sz = cluster1sz
                offset = cluster0sz
            for j in range(0, sz):
                f.write(hostDict[j + offset])
                f.write("\n")
    return ip_list

# Runs the actual experiment
def run(configJson, experimentName, expDir):
    # Load local arguments
    cloudlab = Cloudlab_Experiment()
    config = loadJsonFile(configJson)
    # Name of the experiment that will be run
    cloudlab.experiment_name = experimentName
    # Experiment results dir on the machine
    cloudlab.project_dir = config['experiment_independent_vars']['project_dir']
    # Source directory on the local machine (for compilation)
    cloudlab.src_dir = config['experiment_independent_vars']['src_dir']

    # The nbclients field is a list that contains a list of client counts.
    # Ex, if this is listed: [1,2,4,8], the framework will run the experiment
    # 4 times: one with 1 clients, then with two, then four, then 8. The
    # format for collecting the data will be remoteExpDir/clientcount.
    # If true, simulate latency with tc
    simulateLatency = 0
    try:
        simulateLatency = int(config[experimentName]['simulate_latency'])
    except:
        simulateLatency = 0

    # Setup latency on appropriate hosts if simulated
    if (simulateLatency):
        print("Simulating a " + str(simulateLatency) + " ms")

    increase_packet_size = Scaling_Client_Exp()
    increase_packet_size.nb_rounds = int(config[experimentName]['nb_rounds'])
    # Run for each round, nbRepetitions time.
    
    for i in range(0, increase_packet_size.nb_rounds):
        try:
            # Need to collect the scrooge start commands
            scrooge_commands = []
            clusterZerosz = int(config[experimentName]['scrooge_args']['cluster_0']['local_num_nodes'][i])
            clusterOnesz = int(config[experimentName]['scrooge_args']['cluster_1']['local_num_nodes'][i])
            cluster_zero = config['experiment_independent_vars']['clusterZeroIps']
            cluster_one = config['experiment_independent_vars']['clusterOneIps']
            ip_list =  cluster_zero + cluster_one
            scrooge_exec = "/proj/ove-PG0/reggie/BFT-RSM/Code/scrooge "
            groupId = 0
            nodeId = 0
            for j in range(0, clusterZerosz + clusterOnesz):
                cmd = scrooge_exec + configJson + " " + experimentName + " " + str(groupId) + " " + str(nodeId) + " " + str(i)
                nodeId += 1
                if nodeId == clusterZerosz:
                    nodeId = 0
                    groupId = 1
                scrooge_commands.append(cmd)
            print(scrooge_commands)
            print("Execute command now")
            #import pdb; pdb.set_trace()
            executeParallelBlockingDifferentRemoteCommands(ip_list, scrooge_commands)
            for node_id, ip in enumerate(cluster_zero):
                cluster_id = 0
                file_name = f'log_{cluster_id}_{node_id}'
                executeCommand(f'scp {ip}:/tmp/{file_name}.yaml {expDir}{file_name}_{i}.yaml')
            for node_id, ip in enumerate(cluster_one):
                cluster_id = 1
                file_name = f'log_{cluster_id}_{node_id}'
                executeCommand(f'scp {ip}:/tmp/{file_name}.yaml {expDir}{file_name}_{i}.yaml')
        except Exception as e:
            print(e)
