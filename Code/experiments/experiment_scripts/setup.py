#!/usr/bin/env python3
# Written: Natacha Crooks ncrooks@cs.utexas.edu 2017
# Edited: Micah Murray micahmurray@berekely.edu 2023
# Edited: Reginald Frank reginaldfrank77@berkeley.edu 2023

# Setup - Sets up folders/binaries on all machines

import argparse
import os
import os.path
import sys
from typing import List
from dataclasses import dataclass

# Include all utility scripts
setup_dir = os.path.realpath(os.path.dirname(__file__))
sys.path.append(setup_dir + "/util/")
from ssh_util import *
from json_util import *

@dataclass
class CliArguments:
    setup_dir_path: str
    experiment_config_path: str

def parse_cli_args(argv: List[str]) -> CliArguments:
    parser = argparse.ArgumentParser(description='Sets up a cluster of nodes for experiments')
    parser.add_argument('setup_dir_path', type=str,
                    help='The path of the setup directory contatining a setup.sh script')

    parser.add_argument('experiment_config_path', type=str,
                        help='The path of the experiment configuration json file')
    
    args = parser.parse_args()

    assert os.path.isdir(args.setup_dir_path), f'setup_dir_path {args.setup_dir_path} not found.'
    assert os.path.isfile(args.experiment_config_path), f'experiment_config_path {args.experiment_config_path} not found.'

    return CliArguments(
        setup_dir_path=args.setup_dir_path,
        experiment_config_path=args.experiment_config_path
    )

def get_network_urls(expeirment_config) -> List[str]:
    return expeirment_config['experiment_independent_vars']['clusterZeroIps'] + expeirment_config['experiment_independent_vars']['clusterOneIps'] + ['127.0.0.1']

# Function that setups up appropriate folders on the
# correct machines, and sends the jars. It assumes
# that the appropriate VMs/machines have already started
def setup(cli_arguments: CliArguments):
    print("Starting Setup")

    expeirment_config = loadJsonFile(cli_arguments.experiment_config_path)
    assert expeirment_config, f'Empty Config found at {cli_arguments.experiment_config_path}, failing'
    
    network_urls = get_network_urls(expeirment_config)
    assert len(network_urls) > 1, f'No urls found in the experiment config at {cli_arguments.experiment_config_path}, failing'
    print(f'Ip List: {network_urls}')

    # Copy Setup directory to each machine
    remote_setup_dir = f'/tmp/scrooge_setup'

    for url in network_urls:
        executeCommand(f'scp -o StrictHostKeyChecking=no -r {cli_arguments.setup_dir_path} {url}:{remote_setup_dir} >/dev/null 2>&1')

    # Make bash the default terminal
    executeParallelBlockingRemoteCommand(network_urls, "sudo chsh $SUDO_USER -s /bin/bash 2>&1")

    # Run function to install all appropriate packages on servers
    setup_command = f'sudo {remote_setup_dir}/setup.sh >/dev/null 2>&1'
    executeParallelBlockingRemoteCommand(network_urls, setup_command)

def main():
    cli_arguments = parse_cli_args()
    print("Read arguments:", cli_arguments)
    setup(cli_arguments)

if __name__ == "__main__":
    main()
