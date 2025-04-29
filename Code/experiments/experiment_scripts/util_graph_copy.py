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
    param_seq: List[ExperimentParameters]

@dataclass
class GraphSpec:
    name: str
    line_specs: List[LineSpec]

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
            f'{params.byz_mode}_BYZ',
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

def get_unique_experiment_parameters(graphs: List[GraphSpec]) -> List[ExperimentParameters]:
    """
    Returns a list of unique ExperimentParameters from the given graphs.
    """
    return list(
        set(
            chain.from_iterable(
                chain.from_iterable(
                    [[line.param_seq for line in g.line_specs] for g in graphs]
                )
            )
        )
    )

def get_no_failure_file_graphs() -> List[GraphSpec]:
    """
    Returns a list of GraphSpec objects for experiments without failures.
    """
    # Define the baseline transfer strategies and parameters
    # These are the same as in the original code
    baseline_transfer_strategies = ['SCROOGE', 'ATA', 'OST', 'OTU', 'LL', 'KAFKA']
    baseline_num_nodes = [4, 7, 10, 13, 16, 19]
    baseline_num_bytes = [100, 1000, 10000, 100000, 1000000]

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
        

    # All strategies 1MB, vary network size
    baseline_ii = GraphSpec(
        name="Throughput vs Network Size @ 1MB-messages",
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
                        num_bytes=1000000,
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

    # All strategies 4 node, vary message size
    baseline_iii = GraphSpec(
        name="Throughput vs Message Size @ 4-node-network",
        line_specs=[
            LineSpec(
                name=f"{strategy_name}",
                x_axis_id="num_bytes",
                y_axis_id="throughput",
                param_seq=[
                    ExperimentParameters(
                        strategy_name=strategy_name,
                        system_1="FILE",
                        system_2="FILE",
                        stake_split=1,
                        num_nodes=4,
                        phi_size=256,
                        num_bytes=num_bytes,
                        simulate_crash=False,
                        byz_mode="NO",
                        simulate_throttle=False,
                        run_dr=False,
                        run_ccf=False,
                    )
                    for num_bytes in baseline_num_bytes
                ],
            )
            for strategy_name in baseline_transfer_strategies
        ],
    )

    # All strategies 19 node, vary message size
    baseline_iv = GraphSpec(
        name="Throughput vs Message Size @ 19-node-network",
        line_specs=[
            LineSpec(
                name=f"{strategy_name}",
                x_axis_id="num_bytes",
                y_axis_id="throughput",
                param_seq=[
                    ExperimentParameters(
                        strategy_name=strategy_name,
                        system_1="FILE",
                        system_2="FILE",
                        stake_split=1,
                        num_nodes=19,
                        phi_size=256,
                        num_bytes=num_bytes,
                        simulate_crash=False,
                        byz_mode="NO",
                        simulate_throttle=False,
                        run_dr=False,
                        run_ccf=False,
                    )
                    for num_bytes in baseline_num_bytes
                ],
            )
            for strategy_name in baseline_transfer_strategies
        ],
    )
    
    return [
        baseline_i,
        baseline_ii,
        baseline_iii,
        baseline_iv,
    ]
    
def get_stake_graphs() -> List[GraphSpec]:
    stake_stake_splits = [1, 2, 4, 8, 16, 32, 64]
    stake_num_nodes = [4, 7, 10, 13, 16, 19]

    unthrottled_exp_params = [
        LineSpec(
            name=f"SCROOGE_SPLIT_{stake_split}",
            x_axis_id="num_nodes",
            y_axis_id="throughput",
            param_seq=[
                ExperimentParameters(
                    strategy_name="SCROOGE",
                    system_1="FILE",
                    system_2="FILE",
                    stake_split=stake_split,
                    num_nodes=num_nodes,
                    phi_size=256,
                    num_bytes=100,
                    simulate_crash=False,
                    byz_mode="NO",
                    simulate_throttle=False,
                    run_dr=False,
                    run_ccf=False,
                )
                for num_nodes in stake_num_nodes
            ],
        )
        for stake_split in stake_stake_splits
    ]

    throttled_exp_params = [
        LineSpec(
            name=f"SCROOGE_SPLIT_{stake_split} THROTTLED",
            x_axis_id="num_nodes",
            y_axis_id="throughput",
            param_seq=[
                ExperimentParameters(
                    strategy_name="SCROOGE",
                    system_1="FILE",
                    system_2="FILE",
                    stake_split=stake_split,
                    num_nodes=num_nodes,
                    phi_size=256,
                    num_bytes=100,
                    simulate_crash=False,
                    byz_mode="NO",
                    simulate_throttle=True,
                    run_dr=False,
                    run_ccf=False,
                )
                for num_nodes in stake_num_nodes
            ],
        )
        for stake_split in stake_stake_splits
    ]

    stake_7_i = GraphSpec(
        name="Throughput vs Num Nodes with Varying Stake Split",
        line_specs=throttled_exp_params + unthrottled_exp_params
    )
    return [stake_7_i]

def get_crash_graphs() -> List[GraphSpec]:
    failure_transfer_strategies = ['SCROOGE', 'ATA', 'OTU', 'LL', 'KAFKA']
    failure_phi_sizes = [0, 64, 128, 192, 256]
    failure_network_sizes = [4, 7, 10, 13, 16, 19]

    byz_modes = ["INF", "ZERO", "DELAY"]


    figure_8_crash = GraphSpec(
        name="Crash Failures: Throughput vs Network Size",
        line_specs=[
            LineSpec(
                name=f"{strategy_name}",
                x_axis_id="num_bytes",
                y_axis_id="throughput",
                param_seq=[
                    ExperimentParameters(
                        strategy_name=strategy_name,
                        system_1="FILE",
                        system_2="FILE",
                        stake_split=1,
                        num_nodes=num_nodes,
                        phi_size=256,
                        num_bytes=1000000,
                        simulate_crash=True,
                        byz_mode="NO",
                        simulate_throttle=False,
                        run_dr=False,
                        run_ccf=False,
                    )
                    for num_nodes in failure_network_sizes
                ],
            )
            for strategy_name in failure_transfer_strategies
        ],
    )

    figure_8_phi_scaling = GraphSpec(
        name="Crash Failures -- Phi Scaling: Throughput vs Network Size",
        line_specs=[
            LineSpec(
                name=f"Scrooge phi={phi_size}",
                x_axis_id="num_bytes",
                y_axis_id="throughput",
                param_seq=[
                    ExperimentParameters(
                        strategy_name="SCROOGE",
                        system_1="FILE",
                        system_2="FILE",
                        stake_split=1,
                        num_nodes=num_nodes,
                        phi_size=phi_size,
                        num_bytes=1000000,
                        simulate_crash=True,
                        byz_mode="NO",
                        simulate_throttle=False,
                        run_dr=False,
                        run_ccf=False,
                    )
                    for num_nodes in failure_network_sizes
                ],
            )
            for phi_size in failure_phi_sizes
        ],
    )

    ata_byz_sim_line = LineSpec(
        name="ATA",
        x_axis_id="num_bytes",
        y_axis_id="throughput",
        param_seq=[
            ExperimentParameters(
                strategy_name="ATA",
                system_1="FILE",
                system_2="FILE",
                stake_split=1,
                num_nodes=num_nodes,
                phi_size=256,
                num_bytes=1000000,
                simulate_crash=True, # For A2A, crashing is the only meaningful byz action
                byz_mode="NO",
                simulate_throttle=False,
                run_dr=False,
                run_ccf=False,
            )
            for num_nodes in failure_network_sizes
        ],
    )

    figure_8_byz_actions = GraphSpec(
        name="Byzantine Failures -- Phi Scaling: Throughput vs Network Size",
        line_specs=[
            LineSpec(
                name=f"Scrooge Byzantine_MODE={byz_mode}",
                x_axis_id="num_bytes",
                y_axis_id="throughput",
                param_seq=[
                    ExperimentParameters(
                        strategy_name="SCROOGE",
                        system_1="FILE",
                        system_2="FILE",
                        stake_split=1,
                        num_nodes=num_nodes,
                        phi_size=256,
                        num_bytes=1000000,
                        simulate_crash=True,
                        byz_mode=byz_mode,
                        simulate_throttle=False,
                        run_dr=False,
                        run_ccf=False,
                    )
                    for num_nodes in failure_network_sizes
                ],
            )
            for byz_mode in byz_modes
        ] + [ata_byz_sim_line],
    )
    return [
        figure_8_crash,
        figure_8_phi_scaling,
        figure_8_byz_actions,
    ]

def get_dr_ccf_graphs() -> List[GraphSpec]:
    # picked to give etcd good disk utilization
    application_num_bytes = [245, 498, 863, 1980, 4020, 8052, 14304]
    application_transfer_strategies = ['SCROOGE', 'ATA', 'OST', 'OTU', 'LL', 'KAFKA']

    figure_9_i_dr = GraphSpec(
        name="Disaster Recovery: Throughput vs Message Size",
        line_specs=[
            LineSpec(
                name=f"{strategy_name}",
                x_axis_id="num_bytes",
                y_axis_id="MBps",
                param_seq=[
                    ExperimentParameters(
                        strategy_name=strategy_name,
                        system_1="RAFT",
                        system_2="RAFT",
                        stake_split=1,
                        num_nodes=5,
                        phi_size=256,
                        num_bytes=num_bytes,
                        simulate_crash=False,
                        byz_mode="NO",
                        simulate_throttle=False,
                        run_dr=True,
                        run_ccf=False,
                    )
                    for num_bytes in application_num_bytes
                ],
            )
            for strategy_name in application_transfer_strategies
        ],
    )

    figure_9_ii_ccf = GraphSpec(
        name="Data Reconciliation: Throughput vs Message Size",
        line_specs=[
            LineSpec(
                name=f"{strategy_name}",
                x_axis_id="num_bytes",
                y_axis_id="MBps",
                param_seq=[
                    ExperimentParameters(
                        strategy_name=strategy_name,
                        system_1="RAFT",
                        system_2="RAFT",
                        stake_split=1,
                        num_nodes=5,
                        phi_size=256,
                        num_bytes=num_bytes,
                        simulate_crash=False,
                        byz_mode="NO",
                        simulate_throttle=False,
                        run_dr=False,
                        run_ccf=True,
                    )
                    for num_bytes in application_num_bytes
                ],
            )
            for strategy_name in application_transfer_strategies
        ],
    )
    
    return [
        figure_9_i_dr,
        figure_9_ii_ccf,
    ]
    
    
def get_all_graphspecs() -> List[GraphSpec]:
    return (
        get_no_failure_file_graphs()
        + get_stake_graphs()
        + get_crash_graphs()
        + get_dr_ccf_graphs()
    )