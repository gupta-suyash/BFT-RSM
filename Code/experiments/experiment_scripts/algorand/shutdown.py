#!/usr/bin/env python3
import sys
import os
import subprocess
import threading
import socket
import datetime
import time
import random
import multiprocessing
import concurrent.futures

setup_dir = os.path.realpath(os.path.dirname(__file__))
sys.path.append("../util/")
from ssh_util import *

shutdown_script = "/scripts/shutdown.sh"

def main():
    if len(sys.argv) != 2:
        sys.stderr.write('Usage: python3 %s <script_pathname>\n' % sys.argv[0])
        sys.exit(1)
    script_pathname = sys.argv[1]
    shutdown_script = ". " + script_pathname + shutdown_script + " " + script_pathname
    executeCommand(shutdown_script)

if __name__ == "__main__":
    main()