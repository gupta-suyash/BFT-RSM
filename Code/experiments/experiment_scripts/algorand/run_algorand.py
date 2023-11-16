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

sys.path.append("../util/")
from json_util import *
from ssh_util import *

run_script="/scripts/run.sh"
wallet_name="default"

def main():
    if len(sys.argv) != 3:
        sys.stderr.write('Usage: python3 %s <app_pathname> <script_pathname> <send_acct> <receive_acct> <client_ip>')
        sys.exit(1)
    update_wallets(sys.argv[1], sys.argv[3], sys.argv[4], sys.argv[5])
    executeCommand(pathname + "/run.sh")

if __name__ == "__main__":
    main()