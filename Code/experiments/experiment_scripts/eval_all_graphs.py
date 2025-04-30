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
from itertools import chain
from statistics import median

from eval import *
from util_graph_copy import *

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

def median_y_by_x(points):
    xs = []
    grouped_xs = []
    
    # Group y-values by x
    for x, y in points:
        if x in xs:
            index = xs.index(x)
            grouped_xs[index].append(y)
        else:
            xs.append(x)
            grouped_xs.append([y])
    
    # Compute median y for each x
    result = [(x, median(grouped_xs[i])) for i, x in enumerate(xs)]
    
    # Optional: sort by x for consistency
    result.sort()
    return result

def generate_line(line_spec: LineSpec, dataframe: pd.DataFrame) -> Line:
    """
    Generate a line from the given line specification and dataframe.

    Args:
        line_spec (LineSpec): The line specification.
        dataframe (pd.DataFrame): The dataframe containing the data.

    Returns:
        Line: The generated line.
    """
    experiment_identifiers = list(map(get_exp_string, line_spec.param_seq))
    results = dataframe.query("exp_param_key in @experiment_identifiers")
    
    # Get X axis values
    if line_spec.x_axis_id == 'num_nodes':
        x_axis = results['local_network_size'].to_list()
    elif line_spec.x_axis_id == 'num_bytes':
        x_axis = results['message_size'].to_list()
    else:
        raise ValueError(f"Unknown x_axis_id: {line_spec.x_axis_id}")
    
    # Get Y axis values
    if line_spec.y_axis_id == 'throughput':
        y_axis = (results.max_quorum_acknowledgment - results.starting_quack) / results.duration_seconds
    elif line_spec.y_axis_id == 'MBps':
        y_axis = results.message_size * (results.max_quorum_acknowledgment - results.starting_quack) / results.duration_seconds
    else:
        raise ValueError(f"Unknown y_axis_id: {line_spec.y_axis_id}")
    
    # Sort the x and y values based on the x axis
    xy_values = list(zip(x_axis, y_axis))
    cleaned_values = median_y_by_x(xy_values)
    x_values = [x for x, y in cleaned_values]
    y_values = [y for x, y in cleaned_values]
    
    return Line(
        x_values = x_values,
        y_values = y_values,
        name = line_spec.name,
    )
    

def generate_graph(graph_spec: GraphSpec, dataframe: pd.DataFrame) -> Graph:
    """
    Generate a graph from the given graph specification and dataframe.

    Args:
        graph_spec (GraphSpec): The graph specification.
        dataframe (pd.DataFrame): The dataframe containing the data.

    Returns:
        Graph: The generated graph.
    """
    lines = list(filter(lambda x: x.x_values and x.y_values, map(lambda line_spec: generate_line(line_spec, dataframe), graph_spec.line_specs)))
    x_axis_name = graph_spec.line_specs[0].x_axis_id
    y_axis_name = graph_spec.line_specs[0].y_axis_id
    
    
    return Graph(
        title=graph_spec.name,
        x_axis_name=x_axis_name,
        y_axis_name=y_axis_name,
        lines=lines,
    )

def graph_to_text(graph: Graph) -> str:
    if not graph.lines:
        return f"Graph Title: {graph.title}\n(No data available)"

    output = [f"Graph Title: {graph.title}"]

    # Extract shared x-values
    x_values = graph.lines[0].x_values
    all_y_values = [line.y_values for line in graph.lines]

    # Format values
    formatted_x = [f"{x:.3e}" for x in x_values]
    formatted_ys = [[f"{y:.3e}" for y in line.y_values] for line in graph.lines]

    # Determine max width for alignment
    col_width = max(
        max(len(val) for val in formatted_x),
        *(max(len(val) for val in y_list) for y_list in formatted_ys)
    ) + 2  # extra padding

    max_padding = max(
        [len(f"{graph.x_axis_name} values:")] +
        [len(f"{line.name} ({graph.y_axis_name}):") for line in graph.lines]
    )

    # Format x-values row
    x_row = f"{graph.x_axis_name} values:".ljust(max_padding) + "".join(val.rjust(col_width) for val in formatted_x)
    output.append(x_row)
    output.append("-" * len(x_row))  # Separator

    # Format each line
    for line, y_vals in zip(graph.lines, formatted_ys):
        label = f"{line.name} ({graph.y_axis_name}):"
        y_row = label.ljust(max_padding) + "".join(val.rjust(col_width) for val in y_vals)
        output.append(y_row)

    return "\n".join(output)


def main():
    results_dir = Path(__file__).resolve().parent.parent / 'results'
    if not results_dir.exists():
        sys.stderr.write(f'Error: {results_dir} does not exist\n')
        sys.exit(1)
    
    graph_specs = get_all_graphspecs()
    
    file_names = []
    for experiment_folder in get_folders_in_dir(str(results_dir)):
        if 'THROTTLE' not in experiment_folder:
            # Hacky check with new file naming scheme
            continue
        for run_folder in get_folders_in_dir(str(results_dir / experiment_folder)):
            new_files = get_log_file_names([str(results_dir / experiment_folder / run_folder)])
            file_names.extend(new_files)
            print(f"found {len(new_files)} files in {results_dir}/{experiment_folder}/{run_folder}")
    print(f"finished collecting {len(file_names)} file_names")
    
    dataframe = make_dataframe(file_names)
    dataframe = clean_dataframe(dataframe)
    
    graphs = [generate_graph(g, dataframe) for g in graph_specs]
    
    save_graphs(graphs)
    
    print("Graphs images saved -- textual representation below:")
    
    with open('graphs.txt', 'w') as f:
        for graph in graphs:
            f.write("⭐️"*40 + "\n" + graph_to_text(graph) + "\n")
    for graph in graphs:
        print("⭐️"*40)
        print(graph_to_text(graph))

if __name__ == '__main__':
    main()