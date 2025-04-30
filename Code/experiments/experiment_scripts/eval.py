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
from concurrent.futures import ProcessPoolExecutor
import time


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
        with open(file_name, 'r') as f:
            log_data = yaml.safe_load(f)
            if log_data and len(log_data) > 1:
                return log_data
    except Exception as e:
        print(f'Unable to parse {file_name} -- is it correct yaml format? Error: {e}')
    return None

def make_dataframe(file_names: List[str]) -> pd.DataFrame:
    print(f"Loading {len(file_names)} files")
    start_time = time.perf_counter()
    with ProcessPoolExecutor() as executor:
        results = list(executor.map(process_file, file_names))
    end_time = time.perf_counter()
    print(f'Finished loading {len(file_names)} files in {end_time - start_time:.2f} seconds')
    
    rows = [r for r in results if r]

    return pd.DataFrame.from_dict(rows).replace(r"[+-]?[Nn][Aa][Nn]", np.NaN, regex=True)

def clean_dataframe(dataframe: pd.DataFrame) -> pd.DataFrame:
    # Place Ack count in QAck count for One to One (quack wasn't recorded)
    one_to_one_rows = dataframe.transfer_strategy == "NSendNRecv Thread One-to-One"
    missing_max_qack = (dataframe.max_quorum_acknowledgment == np.NaN) | (dataframe.max_quorum_acknowledgment == 0)
    missing_starting_qack = (dataframe.starting_quack == np.NaN) | (dataframe.starting_quack == 0)
    dataframe.loc[one_to_one_rows & missing_max_qack, 'max_quorum_acknowledgment'] = dataframe.loc[one_to_one_rows].max_acknowledgment[missing_max_qack]
    dataframe.loc[one_to_one_rows & missing_starting_qack, 'starting_quack'] = dataframe.loc[one_to_one_rows].starting_ack[missing_max_qack]
    
    # For DR ONLY!! -- delete the geobft restults which have no throughput
    missing_throughput = ((dataframe.max_quorum_acknowledgment != np.NaN) & (dataframe.max_quorum_acknowledgment != 0.))
    dataframe = dataframe[missing_throughput]

    dataframe.reset_index(inplace=True, drop=True)

    return dataframe

def get_throughput_latency(title: str, dataframe: pd.DataFrame) -> List[Graph]:
    # latency_lines = []
    throughput_lines = []
    for transfer_strategy, group in dataframe.groupby('transfer_strategy'):
        message_sizes = group.message_size.unique()
        message_sizes.sort()
        average_latencies = []
        overall_throughputs = []
        for message_size in message_sizes:
            size_group = group.query('message_size == @message_size')
            quack_delta = size_group.max_quorum_acknowledgment - size_group.starting_quack
            throughput = quack_delta / size_group.duration_seconds
            # latency = size_group.Latency
            # average_latencies.append(latency.median())
            overall_throughputs.append(throughput.median())
        # latency_lines.append(Line(
        #     x_values = message_sizes / 1000,
        #     y_values = average_latencies,
        #     name = transfer_strategy
        # ))
        throughput_lines.append(Line(
            x_values = message_sizes / 1000,
            y_values = overall_throughputs,
            name = transfer_strategy
        ))

    return [
        # Graph(
        #     title = f'{title} Confirmation Latency',
        #     x_axis_name = 'message size (kilobytes)',
        #     y_axis_name = 'Message Latency (seconds)',
        #     lines = latency_lines
        # ),
        Graph(
            title = f'{title} Confirmation Throughput',
            x_axis_name = 'message size (kilobytes)',
            y_axis_name = 'Message Throughput (messages/seconds)',
            lines = throughput_lines
        )
    ]

def get_throughput_network_sz(title: str, dataframe: pd.DataFrame) -> List[Graph]:
    graphs = []
    for message_size, msg_size_group in dataframe.groupby('message_size'):
        line_name_to_points = {}
        for key, group in msg_size_group.groupby(['local_network_size', 'foreign_network_size', 'transfer_strategy']):
            local_network_size, foreign_network_size, transfer_strategy = key
            if local_network_size != foreign_network_size:
                print(f'Found weird network size pairing: {local_network_size} and {foreign_network_size}')
                continue
            throughput = (group.max_quorum_acknowledgment - group.starting_quack) / group.duration_seconds
            line_name_to_points.setdefault(transfer_strategy, []).append((local_network_size, throughput.median()))

            # if 'scrooge' in transfer_strategy.lower():
            #     throughput = (group.max_acknowledgment - group.starting_ack) / group.duration_seconds
            #     line_name_to_points.setdefault(transfer_strategy + ' Acks', []).append((local_network_size, throughput.median()))
        lines = []
        for line_name, points in line_name_to_points.items():
            points.sort()
            x_values, y_values = zip(*points)
            lines.append(
                Line(
                    x_values=list(x_values),
                    y_values=list(y_values),
                    name=line_name
                )
            )
        graphs.append(
            Graph(
                title=f'throughput_vs_network_size@msg={message_size / 1000}KB',
                x_axis_name='Nodes Per Network (N)',
                y_axis_name=f'Throughput (msgs/s)',
                lines=lines
            )
        )
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
    graphs.extend(get_throughput_latency(name, dataframe))
    graphs.extend(get_throughput_network_sz(name, dataframe))
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

def get_throughput_latency_csv(dataframe: pd.DataFrame) -> pd.DataFrame:
    rows = []
    for key, group in dataframe.groupby(['transfer_strategy', 'message_size', 'local_network_size', 'foreign_network_size']):
        transfer_strategy, message_size, local_network_size, foreign_network_size = key
        throughput = (group.max_quorum_acknowledgment - group.starting_quack) / group.duration_seconds
        rows.append({
            'message_size': message_size,
            'msg_throughput': round(throughput.median(), 2),
            'MBps_throughput': round(throughput.median()*message_size/1024/1024, 2),
            'strategy': 'Scrooge',
            'local_network_size': local_network_size
        })
        # if 'scrooge' in transfer_strategy.lower():
        #     throughput = (group.max_acknowledgment - group.starting_ack) / group.duration_seconds
        #     rows.append({
        #         'message_size': message_size,
        #         'latency':0,
        #         'throughput': throughput.median(),
        #         'strategy': transfer_strategy + " Acks",
        #         'local_network_size': local_network_size,
        #         'foreign_network_size': foreign_network_size
        #     })

    return pd.DataFrame.from_dict(rows)

def main():
    dirs = [dir for dir in sys.argv[1:] if os.path.isdir(dir)]
    if len(sys.argv) <= 1:
        usage('You must input at least one directory')
    file_names = get_log_file_names(dirs)
    dataframe = make_dataframe(file_names)
    dataframe = clean_dataframe(dataframe)
    graphs = get_graphs_by_group(dataframe)
    # save_graphs(graphs)

    csv = get_throughput_latency_csv(dataframe)

    csv.sort_values(by=['message_size', 'local_network_size', 'strategy'], inplace=True)
    print('Cur Results:')
    print(csv)
    # csv.to_csv('cur_results.csv', index=False)

if __name__ == '__main__':
    main()