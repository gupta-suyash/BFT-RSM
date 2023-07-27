#!/usr/bin/env python3
import sys
import os

setup_dir = os.path.realpath(os.path.dirname(__file__))
sys.path.append(setup_dir + "/util/")
from ssh_util import *

def main():
    if len(sys.argv) != 4:
        sys.stderr.write(f'Usage: python3 {sys.argv[0]} start_node end_node "command"')
        sys.exit(1)
    start = int(sys.argv[1])
    end = int(sys.argv[2])
    command = sys.argv[3]

    for node in range(start, end + 1):
        ip = f'10.10.1.{node + 1}'
        executeRemoteCommand(ip, command)


if __name__ == "__main__":
    main()
