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
def setup(configJson):
    print("Setup")
    cloudlab = Cloudlab_Experiment()
    config = loadJsonFile(configJson)
    if not config:
        print("Empty config file, failing")
        return
##### LOADING CONFIG FILE ####
    # Username for ssh-ing.
    cloudlab.user = config['experiment_independent_vars']['username']
    # Name of the experiment that will be run
    cloudlab.experiment_name = config['experiment_independent_vars']['experiment_name']
    # Experiment results dir on the machine
    cloudlab.project_dir = config['experiment_independent_vars']['project_dir']
    # Project dir on the remote machine
    #remoteProjectDir = config['remoteprojectdir']
    # Source directory on the local machine (for compilation)
    cloudlab.src_dir = config['experiment_independent_vars']['src_dir']
    # Path to setup script
    cloudlab.local_setup_script = config['experiment_independent_vars']['local_setup_script']
    # Path to setup script for remote machines
    cloudlab.remote_setup_script = config['experiment_independent_vars']['remote_setup_script']
    # List of IPs for every machine in the cluster
    ip_list = config['experiment_independent_vars']['ip_list']
    print(ip_list)
    # Run function to install all appropriate packages on servers
    # TODO setupServers(cloudlab.local_setup_script, cloudlab.remote_setup_script, ip_list)
    # Compile the program once on the local machine
    compileCode(config['experiment_independent_vars']['local_compile_script'])
    # The experiment folder is generated with the following path:
    # results/experimentName/Date
    # The date is used to distinguish multiple runs of the same experiment
    expFolder = cloudlab.project_dir + cloudlab.experiment_name
    expDir =  expFolder + "/" + datetime.datetime.now().strftime("%Y:%m:%d:%H:%M") + "/"
    config['experiment_independent_vars']["experiments_dir"] = expDir

#### GENERATING EXP DIRECTORY ON ALL MACHINES ####
    print("Creating Experiment directory")
    """
    clientIpList = list()
    for c in properties['clients']:
        clientIpList.append(c)
    for c in clientIpList:
        print c
        mkdirRemote(c, remotePath, clientKeyName)
    """
    executeCommand("mkdir -p " + expDir)

    # Create file with git hash
    executeCommand("cp " + configJson + " " + cloudlab.project_dir)
    gitHash = getGitHash(cloudlab.src_dir)
    print("Saving Git Hash " + str(gitHash))
    executeCommand("touch " + expDir + "/git.txt")
    with open(expDir + "/git.txt", 'ab') as f:
        f.write(gitHash)

# Generate Network files
def generateNetwork(clusters, networkConfigDir):
    for i in range(0, len(clusters)):
        filename = "network" + str(i) + "urls.txt"
        executeCommand("rm " + networkConfigDir + filename)
        executeCommand("touch " + networkConfigDir + filename);
        with open(networkConfigDir + filename, 'w') as f:
            print(clusters[i])
            for j in range(0, len(clusters[i])):
                print(clusters[i][j])
                f.write(clusters[i][j])
                f.write("\n")

# Runs the actual experiment
def run(configJson):
    # Load local arguments
    cloudlab = Cloudlab_Experiment()
    config = loadJsonFile(configJson)
    # Name of the experiment that will be run
    cloudlab.experiment_name = config['experiment_independent_vars']['experiment_name']
    # Experiment results dir on the machine
    cloudlab.project_dir = config['experiment_independent_vars']['project_dir']
    # Source directory on the local machine (for compilation)
    cloudlab.src_dir = config['experiment_independent_vars']['src_dir']
    #logFolders = properties['log_folder']

    # The nbclients field is a list that contains a list of client counts.
    # Ex, if this is listed: [1,2,4,8], the framework will run the experiment
    # 4 times: one with 1 clients, then with two, then four, then 8. The
    # format for collecting the data will be remoteExpDir/clientcount.
    scaling_clients_exp = Scaling_Client_Exp()
    scaling_clients_exp.nb_rounds = int(config["client_scaling_experiment"]['nb_rounds'])
    scaling_clients_exp.num_replicas = int(config["client_scaling_experiment"]['num_replicas'])
    scaling_clients_exp.scaling_factor = int(config["client_scaling_experiment"]['scaling_factor'])
    print("Replicas: ", scaling_clients_exp.num_replicas)
    # If true, simulate latency with tc
    try:
        simulateLatency = int(config["client_scaling_experiment"]['simulate_latency'])
    except:
        simulateLatency = 0

    # Create connections for everyone
    ip_list = config['experiment_independent_vars']['ip_list']

    # Setup latency on appropriate hosts if simulated
    if (simulateLatency):
        print("Simulating a " + str(simulateLatency) + " ms")

    first = True
    dataLoaded = False
    # Run for each round, nbRepetitions time.
    for i in range(0, scaling_clients_exp.nb_rounds):
        time.sleep(10)
        for it in range(0, scaling_clients_exp.nb_rounds):
            time.sleep(10)
            try:
                print("Running Round: " + str(i) + " Iter " + str(it))
                clusters = []
                # Need to collect the scrooge start commands
                scrooge_commands = []
                for i in range(0, int(scaling_clients_exp.num_replicas)):
                     #cmd = "/proj/ove-PG0/murray/Scrooge/Code/scrooge " + configJson + " " server_str
                     print("Replica: ", i)
                     scrooge = Scrooge_Args()
                     server_str = "server_" + str(i)
                     scrooge.use_debug_logs_bool = config["client_scaling_experiment"]["scrooge_args"][server_str]["use_debug_logs_bool"]
                     scrooge.node_id = config["client_scaling_experiment"]["scrooge_args"][server_str]["node_id"]
                     scrooge.group_id = config["client_scaling_experiment"]["scrooge_args"][server_str]["group_id"]
                     scrooge.local_num_nodes = config["client_scaling_experiment"]["scrooge_args"][server_str]["local_num_nodes"]
                     scrooge.foreign_num_nodes = config["client_scaling_experiment"]["scrooge_args"][server_str]["foreign_num_nodes"]
                     scrooge.local_max_nodes_fail = config["client_scaling_experiment"]["scrooge_args"][server_str]["local_max_nodes_fail"]
                     scrooge.foreign_max_nodes_fail = config["client_scaling_experiment"]["scrooge_args"][server_str]["foreign_max_nodes_fail"]
                     scrooge.own_network_id = config["client_scaling_experiment"]["scrooge_args"][server_str]["own_network_id"]
                     scrooge.num_packets = config["client_scaling_experiment"]["scrooge_args"][server_str]["num_packets"]
                     scrooge.packet_size = config["client_scaling_experiment"]["scrooge_args"][server_str]["packet_size"]
                     scrooge.log_path = config["client_scaling_experiment"]["scrooge_args"][server_str]["log_path"]
                     if scrooge.group_id >= len(clusters):
                         clusters.append([])
                         clusters[scrooge.group_id].append(config["client_scaling_experiment"]["scrooge_args"][server_str]["ip"]);
                     else:
                         clusters[scrooge.group_id].append(config["client_scaling_experiment"]["scrooge_args"][server_str]["ip"]);
                     cmd = "/proj/ove-PG0/murray/Scrooge/Code/scrooge " + str(scrooge.use_debug_logs_bool) + " "\
                             + str(scrooge.node_id) + " "\
                             + str(scrooge.group_id) +" "\
                             + str(scrooge.local_num_nodes) + " "\
                             + str(scrooge.foreign_num_nodes) + " "\
                             + str(scrooge.local_max_nodes_fail) + " "\
                             + str(scrooge.foreign_max_nodes_fail) + " "\
                             + str(scrooge.num_packets) + " "\
                             + str(scrooge.packet_size) + " "\
                             + scrooge.log_path
                     print("Print: ", cmd)
                     # cmd = "echo 'hello'"
                     scrooge_commands.append(cmd)
                print("Execute command now")
                print("Cluster: ", clusters)
                generateNetwork(clusters, config['experiment_independent_vars']['network_dir'])
                executeParallelBlockingDifferentRemoteCommands(ip_list, scrooge_commands)
            except Exception as e:
                print(e)

# Cleanup: kills ongoing processes and removes old data
# directory
def cleanup(configJson, ecFile="ec2.json"):
    properties = loadPropertyFile(propertyFile)
    ecProperties = loadPropertyFile(ecFile)
    if not properties or not ecProperties:
        print("Empty property file, failing")
        return
    user = properties['username']

    print("Killing processes")
    clientIpList = list()
    for c in properties['clients']:
        clientIpList.append(c)

    for c in clientIpList:
        try:
            print("Killing " + str(c))
            executeRemoteCommandNoCheck(c, "ps -ef | grep java | grep -v grep | grep -v bash | awk '{print \$2}' | xargs -r kill -9", clientKeyName)
        except Exception  as e:
            print(e)

    print("Removing old experiments")
    remoteFolder= properties['experiment_dir'] + '/'+ experimentName
    for c in clientIpList:
        rmdirRemoteIfExists(c,remoteFolder, clientKeyName)
    #if (deleteTable): deleteDynamoTables(propertyFile)


# TODO Collects the data for the experiment
def collectData(propertyFile, ecFile, localFolder, remoteFolder):
    print("Collect Data")

    properties = loadPropertyFile(propertyFile)
    ecProperties = loadPropertyFile(ecFile)
    if not properties or not ecProperties:
        print("Empty property file, failing")
        return
    clientIpList = list()

    for c in properties['clients']:
        clientIpList.append(c)
    getDirectory(localFolder, clientIpList,remoteFolder, clientKeyName)

# Computes experiment results and outputs all results in results.dat
# For each round in an experiment run the "generateData" method as a separate
# thread TODO
def calculateParallel(propertyFile, localExpDir):
    properties = loadPropertyFile(propertyFile)
    if not properties:
        print("Empty property file, failing")
        return
    nbRounds = len(properties['nbclients'])
    experimentName = properties['experimentname']
    if (not localExpDir):
        localProjectDir = properties['localprojectdir']
        expDir = properties['experiment_dir']
        localExpDir = localProjectDir + "/" + expDir
    threads = list()
    fileHandler = open(localExpDir + "/results.dat", "w+")
    for it in range (0, nbRepetitions):
        time = int(properties['exp_length'])
        manager = multiprocessing.Manager()
        results = manager.dict()
        for i in range(0, nbRounds):
            try:
                nbClients = int(properties['nbclients'][i])
                folderName = localExpDir + "/" + str(nbClients) + "_" + str(it) + "/" + str(nbClients) + "_" + str(it)
                executeCommand("rm -f " + folderName + "/clients.dat")
                fileList = dirList(folderName, False,'dat')
                folderName = folderName + "/clients"
                combineFiles(fileList, folderName + ".dat")
                t = multiprocessing.Process(target=generateData,args=(results,folderName +".dat", nbClients, time))
                threads.append(t)
            except:
                print("No File " + folderName)

        executingThreads = list()
        while (len(threads)>0):
            for c in range(0,2):
                try:
                    t = threads.pop(0)
                except:
                    break
                print("Remaining Tasks " + str(len(threads)))
                executingThreads.append(t)
            for t in executingThreads:
                t.start()
            for t in executingThreads:
                t.join()
            print("Finished Processing Batch")
            executingThreads = list()
        sortedKeys = sorted(results.keys())
        for key in sortedKeys:
            fileHandler.write(results[key])
        fileHandler.flush()
    fileHandler.close()


# Generates data using the math functions available in math_util
# Expects latency to be in the third column of the output file
def generateData(results,folderName, clients, time):
    print("Generating Data for " + folderName)
    result = str(clients) + " "
    result+= str(computeMean(folderName,2)) + " "
    result+= str(computeMin(folderName,2)) + " "
    result+= str(computeMax(folderName,2)) + " "
    result+= str(computeVar(folderName,2)) + " "
    result+= str(computeStd(folderName,2)) + " "
    result+= str(computePercentile(folderName,2,50)) + " "
    result+= str(computePercentile(folderName,2,75)) + " "
    result+= str(computePercentile(folderName,2,90)) + " "
    result+= str(computePercentile(folderName,2,95)) + " "
    result+= str(computePercentile(folderName,2,99)) + " "
    result+= str(computeThroughput(folderName,2,time)) + " \n"
    results[clients]=result


# Plots a throughput-latency graph. This graph assumes the
# data format in calculate() function
# Pass in as argument: a list of tuple (dataName, label)
# and the output to which this should be generated
def plotThroughputLatency(dataFileNames, outputFileName, title = None):
    x_axis = "Throughput(Trx/s)"
    y_axis = "Latency(ms)"
    if (not title):
        title = "Throughput-Latency Graph"
    data = list()
    for x in dataFileNames:
        data.append((x[0], x[1], 11, 1))
    plotLine(title, x_axis,y_axis, outputFileName, data, False, xrightlim=200000, yrightlim=5)


# Plots a throughput. This graph assumes the
# data format in calculate() function
# Pass in as argument: a list of tuple (dataName, label)
# and the output to which this should be generated
def plotThroughput(dataFileNames, outputFileName, title = None):
    x_axis = "Clients"
    y_axis = "Throughput (trx/s)"
    if (not title):
        title = "ThroughputGraph"
    data = list()
    for x in dataFileNames:
        data.append((x[0], x[1], 0, 11))
    plotLine(title, x_axis,y_axis, outputFileName, data, False, xrightlim=300, yrightlim=200000)

# Plots a throughput. This graph assumes the
# data format in calculate() function
# Pass in as argument: a list of tuple (dataName, label)
# and the output to which this should be generated
def plotLatency(dataFileNames, outputFileName, title = None):
    x_axis = "Clients"
    y_axis = "Latency(ms)"
    if (not title):
        title = "LatencyGraph"
    data = list()
    for x in dataFileNames:
        data.append((x[0], x[1], 0, 1))
    plotLine(title, x_axis,y_axis, outputFileName, data, False, xrightlim=300, yrightlim=5)
