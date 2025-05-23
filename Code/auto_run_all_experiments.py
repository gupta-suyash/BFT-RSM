#!/usr/bin/env python3

import os
import sys
import subprocess
from typing import Optional
from termcolor import colored
from dataclasses import dataclass, astuple
from itertools import chain
import _io
from typing import List
from concurrent.futures import ThreadPoolExecutor, wait

from util_bash import *
from util_graph import *

def setup_network(dr_or_ccf_exp: bool, dry_run=False, verbose=True) -> None:
    execute_command(f"./auto_make_nodes.sh {dr_or_ccf_exp}", dry_run=dry_run, verbose=verbose)
    
def shutdown_network(dr_or_ccf_exp: bool, dry_run=False, verbose=True) -> None:
    execute_command(f"./auto_delete_nodes.sh {dr_or_ccf_exp}", dry_run=dry_run, verbose=verbose)
    
def shutdown_current_machine(dry_run=False, verbose=True) -> None:
    execute_command(f"gcloud compute instances stop scrooge-worker-2 --zone us-central1-a", dry_run=dry_run, verbose=verbose)
    
def build_experiment(exp_params: ExperimentParameters, dry_run=False, verbose=True) -> None:
    exp_args = (
        " ".join([f"'{x}'" for x in astuple(exp_params)])
        .replace("True", "true")
        .replace("False", "false")
    )
    experiment_name = get_exp_string(exp_params)
    execute_command(f"./auto_build_scrooge.sh '{experiment_name}' {exp_args} 2>&1 | tee /tmp/BUILD-{experiment_name}.log", dry_run=dry_run, verbose=verbose)
    
    
def run_experiment(exp_params: ExperimentParameters, dry_run=False, verbose=True) -> None:
    exp_args = (
        " ".join([f"'{x}'" for x in astuple(exp_params)])
        .replace("True", "true")
        .replace("False", "false")
    )
    experiment_name = get_exp_string(exp_params)
    execute_command(f"./auto_run_exp.sh '{experiment_name}' {exp_args} 2>&1 | tee /tmp/{experiment_name}.log", dry_run=dry_run, verbose=verbose)

def get_folders_in_dir(directory_path: str) -> List[str]:
  """
    Gets a list of all files in the specified directory.

    Args:
        directory_path (str, optional): The path to the directory. Defaults to the current directory.

    Returns:
        list: A list of strings, where each string is the name of a file in the directory.
              Returns an empty list if the directory does not exist.
  """
  if not os.path.exists(directory_path):
    return []
  return [f for f in os.listdir(directory_path) if os.path.isdir(os.path.join(directory_path, f))]

def get_files_in_dir(directory_path: str) -> List[str]:
  """
    Gets a list of all files in the specified directory.

    Args:
        directory_path (str, optional): The path to the directory. Defaults to the current directory.

    Returns:
        list: A list of strings, where each string is the name of a file in the directory.
              Returns an empty list if the directory does not exist.
  """
  if not os.path.exists(directory_path):
    return []
  return [f for f in os.listdir(directory_path) if os.path.isfile(os.path.join(directory_path, f))]

def run_experiments(exp_params: ExperimentParameters) -> None:
    # Strategy: Build next experiment while running the previous one
    with ThreadPoolExecutor() as executor:
        last_params = None
        for exp_params in exp_params:
            futures = []
            # Move pre-compiled scrooge-next to scrooge
            if "scrooge-next" in get_files_in_dir("."):
                execute_command(f"mv scrooge-next scrooge", verbose=False)
            
            # If we have already compiled a scrooge run the experiment
            if last_params is not None:
                futures.append(executor.submit(run_experiment, last_params))
            futures.append(executor.submit(build_experiment, exp_params))
            
            wait(futures)
            last_params = exp_params
    if "scrooge-next" in get_files_in_dir("."):
            execute_command(f"mv scrooge-next scrooge", verbose=False)
    if last_params is not None:
        run_experiment(last_params)


def main():
    all_graphs = get_condensed_graphspecs(4)
    all_experiment_parameters = get_unique_experiment_parameters(all_graphs)
    
    already_ran_experiments = set(get_folders_in_dir("/home/scrooge/BFT-RSM/Code/experiments/results/"))
    experiments_to_run = [x for x in all_experiment_parameters if get_exp_string(x) not in already_ran_experiments]
    
    local_experiments = [
        exp_params
        for exp_params in experiments_to_run
        if not (exp_params.run_dr or exp_params.run_ccf) and exp_params.strategy_name == "KAFKA"
    ][:1]
    
    geo_experiments = [
        exp_params
        for exp_params in experiments_to_run
        if (exp_params.run_dr or exp_params.run_ccf) and exp_params.strategy_name != "KAFKA"
    ][:0]
    
    print(f"Running {len(local_experiments)} local experiments and {len(geo_experiments)} geo experiments")
    
    if local_experiments:
        # First run the local experiments
        print("⭐️⭐️⭐️⭐️⭐️⭐️⭐️⭐️⭐️⭐️ Deploying Local Machines ⭐️⭐️⭐️⭐️⭐️⭐️⭐️⭐️⭐️⭐️")
        setup_network(dr_or_ccf_exp=False)
        
        print("⭐️⭐️⭐️⭐️⭐️⭐️⭐️⭐️⭐️⭐️ Running Local Experiments ⭐️⭐️⭐️⭐️⭐️⭐️⭐️⭐️⭐️⭐️")
        run_experiments(local_experiments)
            
        print("⭐️⭐️⭐️⭐️⭐️⭐️⭐️⭐️⭐️⭐️ Shutting Down Local Machines ⭐️⭐️⭐️⭐️⭐️⭐️⭐️⭐️⭐️⭐️")
        shutdown_network(dr_or_ccf_exp=False)
    
    if geo_experiments:
        # Then run the geo experiments
        print("⭐️⭐️⭐️⭐️⭐️⭐️⭐️⭐️⭐️⭐️ Deploying dr+ccf Machines ⭐️⭐️⭐️⭐️⭐️⭐️⭐️⭐️⭐️⭐️")
        setup_network(dr_or_ccf_exp=True)
        
        print("⭐️⭐️⭐️⭐️⭐️⭐️⭐️⭐️⭐️⭐️ Running dr+ccf Experiments ⭐️⭐️⭐️⭐️⭐️⭐️⭐️⭐️⭐️⭐️")
        run_experiments(geo_experiments)
            
        print("⭐️⭐️⭐️⭐️⭐️⭐️⭐️⭐️⭐️⭐️ Shutting Down dr+ccf Machines ⭐️⭐️⭐️⭐️⭐️⭐️⭐️⭐️⭐️⭐️")
        shutdown_network(dr_or_ccf_exp=True)
        
        print("⭐️⭐️⭐️⭐️⭐️⭐️⭐️⭐️⭐️⭐️ Shutting Down Current Machine ⭐️⭐️⭐️⭐️⭐️⭐️⭐️⭐️⭐️⭐️")
        shutdown_current_machine()
        
        
if __name__ == "__main__":
    # Check if running in tmux
    # if not is_running_in_tmux():
    #     print("This script should be run inside a tmux session.")
    #     sys.exit(1)

    main()