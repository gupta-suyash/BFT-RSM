#!/usr/bin/env python3

from eval import get_log_file_names, make_dataframe
import argparse
from dataclasses import dataclass
import functools
import os
import pandas as pd
from pandas.api.types import is_string_dtype
from pandas.api.types import is_numeric_dtype
from pprint import pprint
from typing import Optional, Set, Union, Dict, Any

@dataclass
class CliArguments:
    log_directory: str
    desired_columns: Optional[Set[str]]

def parse_cli_args() -> CliArguments:
    parser = argparse.ArgumentParser(description='Outputs the "Average" of a log file. Take mean of numbers, concat strings')
    parser.add_argument('log_directory', type=str,
                    help='The path of the directory containing yaml log files')
    parser.add_argument('-c', '--columns_included', nargs='+', default=[],
                    help='Only include columns listed in output')
    
    args = parser.parse_args()

    assert os.path.isdir(args.log_directory), f'log_directory {args.log_directory} not found.'

    return CliArguments(
        log_directory=args.log_directory,
        desired_columns=set(args.columns_included) if len(args.columns_included) else None
    )

def average_map(dataframe: pd.DataFrame) -> Dict[str, Union[str, int, float]]:
    average_map = {}
    for column in dataframe:
        if column == 'max_quorum_acknowledgment':
            average_map[column] = dataframe[column].dropna().mean() / 20e6
        elif is_numeric_dtype(dataframe[column]):
            average_map[column] = dataframe[column].dropna().mean()
        elif is_string_dtype(dataframe[column]):
            average_map[column] = functools.reduce(
                lambda old, new: old + new + ', ',
                dataframe[column],
                ""
            )
    return average_map

def filter_map(map: Dict[str, Any], desired_keys: Set[str]):
    return dict((key, val) for (key,val) in map.items() if key in desired_keys)

def main():
    args = parse_cli_args()
    file_names = get_log_file_names([args.log_directory])
    df = make_dataframe(file_names)
    assert df.size, "Results were empty"

    avg_map = average_map(df)

    if args.desired_columns is not None:
        avg_map = filter_map(avg_map, args.desired_columns)
    

    print(f'Average Dataframe of path: {args.log_directory}')
    if args.desired_columns is not None:
        print(f'Restricting Columns to {args.desired_columns}')
    else:
        print(f'Printing all columns.')
    pprint(avg_map)

if __name__ == '__main__':
    main()
