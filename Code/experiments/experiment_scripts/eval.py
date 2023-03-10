#!/usr/bin/env python3

import os
from dataclasses import dataclass, astuple
from pathlib import Path
from typing import List
import pandas as pd
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
        file_names += [file for file in os.listdir(path) if file.startswith("log_") and file.endswith(".yaml")]
    return file_names

def make_dataframe(file_names: List[str]) -> pd.DataFrame:
    rows = []
    for file_name in file_names:
        try:
            rows.append(yaml.safe_load(Path(file_name).read_text()))
        except:
            usage(f'Unable to parse {file_name} -- is it correct yaml format?')
    return pd.DataFrame.from_dict(rows)

def get_graphs(dataframe: pd.DataFrame) -> List[Graph]:
    latency_lines = []
    throughput_lines = []
    for transfer_strategy, group in dataframe.groupby('transfer_strategy'):
        message_sizes = group.message_size.unique()
        message_sizes.sort()
        average_latencies = []
        overall_throughputs = []
        for message_size in message_sizes:
            average_latency = dataframe.query('message_size == @message_size').average_latency.mean()
            overall_throughput = dataframe.query('message_size == @message_size').total_throughput.mean()
            average_latencies.append(average_latency)
            overall_throughputs.append(overall_throughput)
        latency_lines.append(Line(
            x_values = message_sizes / 1000,
            y_values = average_latencies,
            name = transfer_strategy
        ))
        throughput_lines.append(Line(
            x_values = message_sizes / 1000,
            y_values = overall_throughputs,
            name = transfer_strategy
        ))

    return [
        Graph(
            title = 'Average Message Delivery Latency',
            x_axis_name = 'message size (kilobytes)',
            y_axis_name = 'Receiver Calculated Message Latency (seconds)',
            lines = latency_lines
        ),
        Graph(
            title = 'Overall Message Throughput',
            x_axis_name = 'message size (kilobytes)',
            y_axis_name = 'Receiver Calculated Message Throughput (messages/seconds)',
            lines = throughput_lines
        )
    ]

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


def main():
    if len(sys.argv) <= 1:
        usage('You must input at least one directory')
    paths = sys.argv[1:]
    file_names = get_log_file_names(paths)
    dataframe = make_dataframe(file_names)
    graphs = get_graphs(dataframe)
    save_graphs(graphs)

if __name__ == '__main__':
    main()
