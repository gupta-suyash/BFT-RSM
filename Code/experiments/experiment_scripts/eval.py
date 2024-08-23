#!/usr/bin/env python3

import os
from dataclasses import dataclass, astuple
from pathlib import Path
from typing import List
import pandas as pd
import numpy as np
import yaml
import sys
import plotly.graph_objects as go
from concurrent.futures import ThreadPoolExecutor

@dataclass
class Line:
    x_values: List[float]
    y_values: List[float]
    name: str

@dataclass
class Graph:
    title: str
    x_axis_name: str
    y_axis_name: str
    lines: List[Line]

def usage(err):
    sys.stderr.write(f'Error: {err}\n')
    sys.stderr.write(f'Usage: ./eval.py directory...\n')
    sys.exit(1)    

def get_log_file_names(paths: List[str]) -> List[str]:
    file_names = []
    for path in paths:
        if not os.path.exists(path):
            usage(f'Unable to find path {path}')
        file_names += [os.path.join(path, file) for file in os.listdir(path) if file.startswith("log_") and file.endswith(".yaml")]
    return file_names

def process_file(file_name: str):
    try:
        file_text = Path(file_name).read_text()
        if file_text:
            log_data = yaml.safe_load(file_text)
            if len(log_data):
                return log_data
    except Exception as e:
        print(f'Unable to parse {file_name} -- is it correct yaml format? Error: {e}')
    return None

def make_dataframe(file_names: List[str]) -> pd.DataFrame:
    with ThreadPoolExecutor() as executor:
        results = list(executor.map(process_file, file_names))

    # Filter out any None values from failed processing
    rows = [result for result in results if result is not None]

    basic_df = pd.DataFrame.from_dict(rows)
    return basic_df.replace("[+-]?[Nn][Aa][Nn]", np.NaN, regex=True)

# modifies dataframe in place
def clean_dataframe(dataframe: pd.DataFrame) -> pd.DataFrame:
    # Place Ack count in QAck count for One to One (quack wasn't recorded)
    one_to_one_rows = dataframe.transfer_strategy == "NSendNRecv Thread One-to-One"
    missing_max_qack = (dataframe.max_quorum_acknowledgment == np.NaN) | (dataframe.max_quorum_acknowledgment == 0)
    missing_starting_qack = (dataframe.starting_quack == np.NaN) | (dataframe.max_quorum_acknowledgment == 0)
    dataframe.loc[one_to_one_rows & missing_max_qack, 'max_quorum_acknowledgment'] = dataframe.loc[one_to_one_rows].max_acknowledgment[missing_max_qack]
    dataframe.loc[one_to_one_rows & missing_starting_qack, 'starting_quack'] = dataframe.loc[one_to_one_rows].starting_ack[missing_max_qack]

def get_max_throughputs_vs_axis(title: str, x_axis_name: str, dataframe: pd.DataFrame) -> List[Graph]:
    tuning_params = ['Ack Window', 'Batch Size', 'Batch Timeout', 'Max message delay', 'Noop Delay', 'Pipeline Buffer Size',
                      'Quack Window', 'message_size', 'socket-buffer-size-receive', 'socket-buffer-size-send', 'kList_size']
    strategy_names = ['Scrooge', 'All-to-All', 'One-to-One']
    print(f'Tuning parameters: {tuning_params}')
    throughput_lines = []
    for strategy_name in strategy_names:
        strategy_results = dataframe[dataframe['transfer_strategy'].str.contains(strategy_name)]
        x_axis_values = strategy_results[x_axis_name].unique()
        not_enough_points_to_graph = len(x_axis_values) < 2
        if not_enough_points_to_graph:
            continue
        x_axis_values.sort()
        overall_throughputs = []
        for x_value in x_axis_values:
            max_throughput = None
            best_params = None
            for parameters, tuned_results in strategy_results.query(f'{x_axis_name} == @x_value').groupby(tuning_params):
                quack_delta = tuned_results.max_quorum_acknowledgment - tuned_results.starting_quack
                throughput = quack_delta / tuned_results.duration_seconds
                cur_throughput = throughput.median()
                if max_throughput is None or max_throughput < cur_throughput:
                    max_throughput = cur_throughput
                    best_params = parameters
            overall_throughputs.append(max_throughput)
            print(f'Best parameters for {strategy_name} @{x_value} {x_axis_name} are'.ljust(65), best_params)
        throughput_lines.append(Line(
            x_values = x_axis_values,
            y_values = overall_throughputs,
            name = strategy_name
        ))

    if throughput_lines:
        return [
            Graph(
                title = f'{title}',
                x_axis_name = x_axis_name,
                y_axis_name = 'Message Throughput (messages/seconds)',
                lines = throughput_lines
            )
        ]
    return []

def get_max_throughput_vs_network_size(dataframe: pd.DataFrame) -> List[Graph]:
    graphs = []
    for message_size, results in dataframe.groupby(['message_size']):
        graphs += get_max_throughputs_vs_axis(f'Local Network Size vs Throughput For Msg Size {message_size[0]} Bytes', 'local_network_size', results)
    return graphs

def get_max_throughput_vs_msg_size(dataframe: pd.DataFrame) -> List[Graph]:
    graphs = []
    for network_sizes, results in dataframe.groupby(['local_network_size', 'foreign_network_size']):
        local_network_size, foreign_network_size = network_sizes
        graphs += get_max_throughputs_vs_axis(f'Msg Size vs Throughput for Local-Foreign network sizes {local_network_size}-{foreign_network_size} ', 'message_size', results)
    return graphs

def get_throughput_bandwidth(dataframe: pd.DataFrame) -> List[Graph]:
    throughput_lines = []
    bandwidth_lines = []
    for transfer_strategy, group in dataframe.groupby('transfer_strategy'):
        message_sizes = group.message_size.unique()
        message_sizes.sort()
        average_throughput = []
        average_bandwidth = []
        for message_size in message_sizes:
            cur_data = group.query('message_size == @message_size')
            #import pdb; pdb.set_trace(); 
            throughput = cur_data.messages_received / cur_data.duration_seconds
            bandwidth = throughput * message_size * 8
            average_throughput.append(throughput.median())
            average_bandwidth.append(bandwidth.median())
        throughput_lines.append(Line(
            x_values = message_sizes / 1024,
            y_values = average_throughput,
            name = transfer_strategy
        ))
        bandwidth_lines.append(Line(
            x_values = message_sizes / 1024,
            y_values = average_bandwidth,
            name = transfer_strategy
        ))

    return [
        Graph(
            title = 'Average Message Throughput',
            x_axis_name = 'message size (KiB)',
            y_axis_name = 'Messages Received (per node) / second',
            lines = throughput_lines
        ),
        Graph(
            title = 'Average Message Bandwidth',
            x_axis_name = 'message size (KiB)',
            y_axis_name = 'Useful Bytes Received (per node) / second',
            lines = bandwidth_lines
        )
    ]

def get_graphs(name: str, dataframe: pd.DataFrame) -> List[Graph]:
    graphs = []
    # graphs.extend(get_throughput_latency(name, dataframe))
    graphs.extend(get_max_throughput_vs_msg_size(dataframe))
    graphs.extend(get_max_throughput_vs_network_size(dataframe))
    # graphs.extend(get_throughput_network_sz(name, dataframe))
    # graphs.extend(get_throughput_bandwidth(dataframe))
    return graphs

def get_graphs_by_group(dataframe: pd.DataFrame):
    return get_graphs('Equal Stake', dataframe)
    dist_to_name = {'11111111':'Equal Stake', '43214321':'4321 Stake', '71117111':'7111 Stake'}
    graphs = []
    for stake_dist, dist_results in dataframe.groupby('stake_dist'):
        graph_name = dist_to_name.get(stake_dist) or f'Stake Dist: {str(stake_dist)}'
        graphs.extend(get_graphs(graph_name, dist_results))
    return graphs

def make_fig(graph: Graph) -> go.Figure: 
    fig = go.Figure()
    
    for line in graph.lines:
        x_values, y_values, name = astuple(line)
        fig.add_trace(
            go.Scatter(
                x=x_values,
                y=y_values,
                mode='lines',
                name=name
            )
        )
    
    fig.update_layout(
        title = graph.title,
        xaxis_title = graph.x_axis_name,
        yaxis_title = graph.y_axis_name,
        showlegend = True
    )

    fig.update_xaxes(type="log")
    fig.update_yaxes(type="log")
    
    return fig

def save_graphs(graphs: List[Graph]):
    for graph in graphs:
        fig = make_fig(graph)
        fig.write_image(f'{graph.title}.png')

def get_max_throughput_df(dataframe: pd.DataFrame) -> pd.DataFrame:
    strategy_names = ['Scrooge', 'All-to-All', 'One-to-One']
    tuning_params = ['Ack Window', 'Batch Size', 'Batch Timeout', 'Max message delay', 'Noop Delay', 'Pipeline Buffer Size',
                      'Quack Window', 'message_size', 'socket-buffer-size-receive', 'socket-buffer-size-send', 'kList_size']
    print(f'Tuning parameters: {tuning_params}')
    rows = []
    for strategy_name in strategy_names:
        strategy_results = dataframe[dataframe['transfer_strategy'].str.contains(strategy_name)]
        for key, experiment_group in strategy_results.groupby(['message_size', 'local_network_size', 'foreign_network_size']):
            message_size, local_network_size, foreign_network_size = key
            best_result = None
            best_tuning_parameters = None
            for tuning_parameters, tuned_experiment_group in experiment_group.groupby(tuning_params):
                
                cur_throughputs = (tuned_experiment_group.max_quorum_acknowledgment - tuned_experiment_group.starting_quack) / tuned_experiment_group.duration_seconds
                cur_throughput = cur_throughputs.median()
                if best_result is None or cur_throughput > best_result['throughput']:
                    best_result = {
                        'message_size': message_size,
                        'throughput': cur_throughput,
                        'strategy': strategy_name,
                        'local_network_size': local_network_size,
                        'foreign_network_size': foreign_network_size
                    }
                    best_tuning_parameters = tuning_parameters
            rows.append(best_result)
            print(f'Best Tuning Parameters for {strategy_name}@ {message_size} Bytes, {local_network_size} nodes:'.ljust(65), best_tuning_parameters)

    return pd.DataFrame.from_dict(rows)

def get_throughput_latency_csv(dataframe: pd.DataFrame) -> pd.DataFrame:
    rows = []
    for key, group in dataframe.groupby(['transfer_strategy', 'message_size', 'local_network_size', 'foreign_network_size']):
        transfer_strategy, message_size, local_network_size, foreign_network_size = key
        throughput = (group.max_quorum_acknowledgment - group.starting_quack) / group.duration_seconds
        rows.append({
            'message_size': message_size,
            'latency':0,
            'throughput': throughput.median(),
            'strategy': transfer_strategy,
            'local_network_size': local_network_size,
            'foreign_network_size': foreign_network_size
        })
        if 'scrooge' in transfer_strategy.lower():
            throughput = (group.max_acknowledgment - group.starting_ack) / group.duration_seconds
            rows.append({
                'message_size': message_size,
                'latency':0,
                'throughput': throughput.median(),
                'strategy': transfer_strategy + " Acks",
                'local_network_size': local_network_size,
                'foreign_network_size': foreign_network_size
            })

    return pd.DataFrame.from_dict(rows)

def main():
    dirs = [dir for dir in sys.argv[1:] if os.path.isdir(dir)]
    if len(sys.argv) <= 1:
        usage('You must input at least one directory')
    file_names = get_log_file_names(dirs)
    dataframe = make_dataframe(file_names)
    clean_dataframe(dataframe)
    graphs = get_graphs_by_group(dataframe)
    save_graphs(graphs)

    csv = get_max_throughput_df(dataframe)

    csv.sort_values(by=['strategy', 'message_size', 'local_network_size'], inplace=True)
    print('Cur Results:')
    print(csv)
    csv.to_csv('cur_results.csv', index=False)

if __name__ == '__main__':
    main()
