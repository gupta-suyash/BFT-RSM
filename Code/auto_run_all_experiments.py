#!/usr/bin/env python3

import os
import sys
import subprocess
from typing import Optional
from termcolor import colored
from dataclasses import dataclass, astuple
from itertools import chain
import _io

def is_running_in_tmux():
    """
    Checks if the current process is running inside a tmux session.

    Returns:
      True if running in tmux, False otherwise.
    """
    return "TMUX" in os.environ


# make command helper have a lock and print out the invocation number
# Then if there is a
def execute_command_helper(
    command: str,
    *,
    dry_run: bool = False,
    verbose: bool = True,
    cmd_out: Optional[_io.TextIOWrapper] = None,
    cmd_err: Optional[_io.TextIOWrapper] = None,
):
    if cmd_out is None:
        output = sys.stdout
    if cmd_err is None:
        cmd_err = sys.stderr

    if dry_run:
        print("Dry Run, normally would execute: ", colored(command, "dark_grey"))
        return
    if verbose:
        print(
            "==================================== Executing command ===================================="
        )
        print(colored(command, "dark_grey"))
        print(
            "------------------------------------------------------------------------"
        )
        print(
            f"Command output going to: cmd_out: {colored(cmd_out.name, 'dark_grey')} cmd_err: {colored(cmd_err.name, 'dark_grey')}"
        )
        print(
            "------------------------------------------------------------------------"
        )

    try:
        subprocess.check_call(command, shell=True, stdout=cmd_out, stderr=cmd_err)
        if verbose:
            print(
                f'Exited command successfully ✅ -- reference:\n {colored(command, "dark_grey")}'
            )
            print(
                "======================================================================================"
            )
            print()
    except Exception as e:
        print(
            f"❌❌❌❌❌❌❌❌❌❌❌❌❌Terminated with error❌❌❌❌❌❌❌❌❌❌❌❌❌❌❌ err_log @ {colored(cmd_err.name, 'dark_grey')}"
        )
        print(colored(str(e), "red"))
        print(
            "======================================================================================"
        )
        print()


def execute_command(
    command: str,
    *,
    dry_run: bool = False,
    verbose: bool = True,
    cmd_out: Optional[str] = None,
    cmd_err: Optional[str] = None,
):
    cmd_out = sys.stdout if cmd_out is None else open(cmd_out, "a")
    cmd_err = sys.stderr if cmd_err is None else open(cmd_err, "a")

    execute_command_helper(
        command, dry_run=dry_run, verbose=verbose, cmd_out=cmd_out, cmd_err=cmd_err
    )

    if cmd_out is not sys.stdout:
        cmd_out.close()
    if cmd_err is not sys.stderr:
        cmd_err.close()
        

@dataclass(frozen=True, eq=True)
class ExperimentParameters:
    strategy_name: str
    system_1: str
    system_2: str
    stake_split: int
    num_nodes: int
    phi_size: int
    num_bytes: int
    simulate_crash: bool
    byz_mode: "NO"
    simulate_throttle: bool
    run_dr: bool
    run_ccf: bool

@dataclass
class LineSpec:
    name: str
    x_axis_id: str # string (usually in each exp_param representing x axis)
    y_axis_id: str # string (usually throughput in each result file representing y axis)
    param_seq: list[ExperimentParameters]

@dataclass
class GraphSpec:
    name: str
    line_specs: list[LineSpec]

def get_exp_string(params: ExperimentParameters) -> str:
    return '-'.join(
        [
            f'{params.strategy_name}',
            f'{params.system_1}_{params.system_2}',
            f'{params.stake_split}_STAKE_SPLIT',
            f'{params.num_nodes}_NUM_NODES',
            f'{params.phi_size}_PHI',
            f'{params.num_bytes}_BYTES',
            f'{"YES" if params.simulate_crash else "NO"}_CRASH',
            f'{"YES" if params.byz_mode else "NO"}_BYZ',
            f'{"YES" if params.simulate_throttle else "NO"}_THROTTLE',
            f'{"YES" if params.run_dr else "NO"}_DR',
            f'{"YES" if params.run_ccf else "NO"}_CCF',
        ]
    )

def parse_exp_string(exp_str: str) -> ExperimentParameters:
    parts = exp_str.split('-')
    
    strategy_name = parts[0]
    system_1, system_2 = parts[1].split('_')
    stake_split = int(parts[2].replace('_STAKE_SPLIT', ''))
    num_nodes = int(parts[3].replace('_NUM_NODES', ''))
    phi_size = int(parts[4].replace('_PHI', ''))
    num_bytes = int(parts[5].replace('_BYTES', ''))

    def parse_bool(part: str, suffix: str) -> bool:
        value = part.replace(f'_{suffix}', '')
        return value == 'YES'

    simulate_crash = parse_bool(parts[6], 'CRASH')
    byz_mode = parse_bool(parts[7], 'BYZ')
    simulate_throttle = parse_bool(parts[8], 'THROTTLE')
    run_dr = parse_bool(parts[9], 'DR')
    run_ccf = parse_bool(parts[10], 'CCF')

    return ExperimentParameters(
        strategy_name=strategy_name,
        system_1=system_1,
        system_2=system_2,
        stake_split=stake_split,
        num_nodes=num_nodes,
        phi_size=phi_size,
        num_bytes=num_bytes,
        simulate_crash=simulate_crash,
        byz_mode=byz_mode,
        simulate_throttle=simulate_throttle,
        run_dr=run_dr,
        run_ccf=run_ccf
    )
    
def setup_network(dr_or_ccf_exp: bool, dry_run=False, verbose=True) -> None:
    execute_command(f"./auto_make_nodes.sh {dr_or_ccf_exp}", dry_run=dry_run, verbose=verbose)
    
def shutdown_network(dr_or_ccf_exp: bool, dry_run=False, verbose=True) -> None:
    execute_command(f"./auto_delete_nodes.sh {dr_or_ccf_exp}", dry_run=dry_run, verbose=verbose)
    
def run_experiment(exp_params: ExperimentParameters) -> None:
    exp_args = (
        " ".join([f"'{x}'" for x in astuple(exp_params)])
        .replace("True", "true")
        .replace("False", "false")
    )
    experiment_name = get_exp_string(exp_params)
    print(f"./auto_make_network.sh '{experiment_name}' {exp_args}")

def get_unique_experiment_parameters(graphs: list[GraphSpec]) -> list[ExperimentParameters]:
    """
    Returns a list of unique ExperimentParameters from the given graphs.
    """
    return list(
        set(
            chain.from_iterable(
                chain.from_iterable(
                    [[line.param_seq for line in g.line_specs] for g in testing_graph_specs]
                )
            )
        )
    )

def get_no_failure_file_graphs() -> list[GraphSpec]:
    """
    Returns a list of GraphSpec objects for experiments without failures.
    """
    # Define the baseline transfer strategies and parameters
    # These are the same as in the original code
    baseline_transfer_strategies = ['SCROOGE', 'ATA', 'OST', 'OTU', 'LL', 'KAFKA']
    baseline_num_nodes = [4, 10, 19]
    baseline_num_bytes = [100, 10000, 1000000]

    # All strategies, 100B, vary network size
    baseline_i = GraphSpec(
        name="Throughput vs Network Size @ 100B-messages",
        line_specs=[
            LineSpec(
                name=f"{strategy_name}",
                x_axis_id="num_nodes",
                y_axis_id="throughput",
                param_seq=[
                    ExperimentParameters(
                        strategy_name=strategy_name,
                        system_1="FILE",
                        system_2="FILE",
                        stake_split=1,
                        num_nodes=num_nodes,
                        phi_size=256,
                        num_bytes=100,
                        simulate_crash=False,
                        byz_mode="NO",
                        simulate_throttle=False,
                        run_dr=False,
                        run_ccf=False,
                    )
                    for num_nodes in baseline_num_nodes
                ],
            )
            for strategy_name in baseline_transfer_strategies
        ],
    )
    
    return [baseline_i]


def main():
    all_graphs = get_no_failure_file_graphs()
    all_experiment_parameters = get_unique_experiment_parameters(all_graphs)
    
    local_experiments = [
        exp_params
        for exp_params in all_experiment_parameters
        if not (exp_params.run_dr or exp_params.run_ccf)
    ]
    
    geo_experiments = [
        exp_params
        for exp_params in all_experiment_parameters
        if exp_params.run_dr or exp_params.run_ccf
    ]
    
    # FOR TESTING
    local_experiments = [local_experiments[0]]
    
    # First run the local experiments
    setup_network(dr_or_ccf_exp=False)
    for exp_params in local_experiments:
        run_experiment(exp_params)
    shutdown_network(dr_or_ccf_exp=False)
    
    return
    
    # Then run the geo experiments
    setup_network(dr_or_ccf_exp=True)
    for exp_params in geo_experiments:
        run_experiment(exp_params)
    shutdown_network(dr_or_ccf_exp=True)
        
        
if __name__ == "__main__":
    # Check if running in tmux
    if not is_running_in_tmux():
        print("This script should be run inside a tmux session.")
        sys.exit(1)

    main()