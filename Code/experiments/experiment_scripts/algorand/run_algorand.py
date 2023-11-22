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
import json

sys.path.append("/home/scrooge/BFT-RSM/Code/experiments/experiment_scripts/util/")
from json_util import *
from ssh_util import *

port="4161"
run_script="/scripts/run.sh"
wallet_name="default"

def main():
    if len(sys.argv) < 3:
        sys.stderr.write('Usage: python3 %s <app_pathname> <script_pathname> <port>')
        sys.exit(1)
    subprocess.check_call([". " + sys.argv[2] + run_script, sys.argv[1], sys.argv[2], port, wallet_name], shell=True, stdout=sys.stdout, stderr=sys.stdout)

if __name__ == "__main__":
    main()
