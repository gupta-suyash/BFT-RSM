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

def make_dataframe(file_names: List[str]) -> pd.DataFrame:
    rows = []
    for file_name in file_names:
        try:
            yaml_map = yaml.safe_load(Path(file_name).read_text())
            if yaml_map is not None:
                rows.append(yaml_map)
        except:
            usage(f'Unable to parse {file_name} -- is it correct yaml format?')
    return pd.DataFrame.from_dict(rows)

# modifies dataframe in place
def clean_dataframe(dataframe: pd.DataFrame) -> pd.DataFrame:
    # Place Ack count in QAck count for One to One (quack wasn't recorded)
    one_to_one_rows = dataframe.transfer_strategy == "One-to-One"
    missing_max_qack = (dataframe.max_quorum_acknowledgment == np.NaN) | (dataframe.max_quorum_acknowledgment == 0)
    missing_starting_qack = (dataframe.starting_quack == np.NaN) | (dataframe.max_quorum_acknowledgment == 0)
    dataframe.loc[one_to_one_rows & missing_max_qack, 'max_quorum_acknowledgment'] = dataframe.loc[one_to_one_rows].max_acknowledgment[missing_max_qack]
    dataframe.loc[one_to_one_rows & missing_starting_qack, 'starting_quack'] = dataframe.loc[one_to_one_rows].starting_ack[missing_max_qack]

def get_throughput_latency(title: str, dataframe: pd.DataFrame) -> List[Graph]:
    latency_lines = []
    throughput_lines = []
    for transfer_strategy, group in dataframe.groupby('transfer_strategy'):
        message_sizes = group.message_size.unique()
        message_sizes.sort()
        average_latencies = []
        overall_throughputs = []
        x_vals = []
        for message_size in message_sizes:
            if message_size <= 50000:
                continue
            size_group = group.query('message_size == @message_size')
            quack_delta = size_group.max_quorum_acknowledgment - size_group.starting_quack
            throughput = quack_delta / size_group.duration_seconds
            latency = size_group.Latency
            average_latencies.append(latency.mean())
            overall_throughputs.append(throughput.mean())
            x_vals.append(message_size)
        latency_lines.append(Line(
            x_values = np.array(x_vals) / 1000,
            y_values = average_latencies,
            name = transfer_strategy
        ))
        throughput_lines.append(Line(
            x_values = np.array(x_vals) / 1000,
            y_values = overall_throughputs,
            name = transfer_strategy
        ))

    return [
        Graph(
            title = f'{title} Confirmation Latency',
            x_axis_name = 'message size (kilobytes)',
            y_axis_name = 'Message Latency (seconds)',
            lines = latency_lines
        ),
        Graph(
            title = f'{title} Confirmation Throughput',
            x_axis_name = 'message size (kilobytes)',
            y_axis_name = 'Message Throughput (messages/seconds)',
            lines = throughput_lines
        )
    ]

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
            average_throughput.append(throughput.mean())
            average_bandwidth.append(bandwidth.mean())
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
    graphs.extend(get_throughput_latency(name, dataframe))
    # graphs.extend(get_throughput_bandwidth(dataframe))
    return graphs

def get_graphs_by_group(dataframe: pd.DataFrame) -> List[Graph]:
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
    
    return fig

def save_graphs(graphs: List[Graph]):
    for graph in graphs:
        fig = make_fig(graph)
        fig.write_image(f'{graph.title}.png')

def get_throughput_latency_csv(dataframe: pd.DataFrame) -> pd.DataFrame:
    rows = []
    for transfer_strategy, group in dataframe.groupby('transfer_strategy'):
        message_sizes = group.message_size.unique()
        message_sizes.sort()
        average_latencies = []
        overall_throughputs = []
        for message_size in message_sizes:
            size_group = group.query('message_size == @message_size')
            quack_delta = size_group.max_quorum_acknowledgment - size_group.starting_quack
            throughput = quack_delta / size_group.duration_seconds
            latency = size_group.Latency
            rows.append({
                'message_size': message_size,
                'latency': latency.mean(),
                'throughput': throughput.mean(),
                'strategy': transfer_strategy
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

    csv = get_throughput_latency_csv(dataframe)

    csv.sort_values(by=['strategy', 'message_size'], inplace=True)
    print('Cur Results:')
    print(csv)
    csv.to_csv('cur_results.csv', index=False)

if __name__ == '__main__':
    main()
