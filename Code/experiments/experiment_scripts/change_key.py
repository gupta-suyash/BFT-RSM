#!/usr/bin/env python3

import os
from dataclasses import dataclass
from pathlib import Path
from typing import Union
import yaml
import sys
import re


def usage(err):
    sys.stderr.write(f'Error: {err}\n')
    sys.stderr.write(f'Usage: ./eval.py directory...\n')
    sys.exit(1)

def is_int(string: str) -> bool:
    pattern = r"[-+]?\d+"
    match = re.match(pattern, string)
    return bool(match)
 
def is_float(string: str) -> bool:
    pattern = r"[-+]?(\d+(\.\d*)?|\.\d+)([eE][-+]?\d+)?"
    match = re.match(pattern, string)
    return bool(match)

def change_key(file_name: str, key: str, value: Union[str, float, int]) -> None:
    file_yaml = yaml.safe_load(Path(file_name).read_text())
    file_yaml[key] = value
    with open(file_name, 'w') as file:
        yaml.dump(file_yaml, file)

def main():
    if len(sys.argv) < 1 + 3:
        usage('run as ./change_key "key_name" <value> file...')
    key = sys.argv[1]
    value = sys.argv[2]
    if is_int(value):
        value = int(value)
    elif is_float(value):
        value = float(value)
        
    files = [file for file in sys.argv[3:] if os.path.isfile(file) and file.startswith("log_") and file.endswith(".yaml")]
    if len(files) == 0:
        usage('You must input at least one log file')
    
    for file in files:
        change_key(file, key, value)

if __name__ == '__main__':
    main()
