#!/usr/bin/env python3

import os
import sys
import subprocess
from typing import Optional
from termcolor import colored
from dataclasses import dataclass, astuple
from itertools import chain
import _io
from typing import List

def is_running_in_tmux():
    """
    Checks if the current process is running inside a tmux session.

    Returns:
      True if running in tmux, False otherwise.
    """
    return "TMUX" in os.environ


# make command helper have a lock and print out the invocation number
# Then if there is a
def execute_command_helper(
    command: str,
    *,
    dry_run: bool = False,
    verbose: bool = True,
    cmd_out: Optional[_io.TextIOWrapper] = None,
    cmd_err: Optional[_io.TextIOWrapper] = None,
):
    if cmd_out is None:
        output = sys.stdout
    if cmd_err is None:
        cmd_err = sys.stderr

    if dry_run:
        print("Dry Run, normally would execute: ", colored(command, "dark_grey"))
        return
    if verbose:
        print(
            "==================================== Executing command ===================================="
        )
        print(colored(command, "dark_grey"))
        print(
            "------------------------------------------------------------------------"
        )
        print(
            f"Command output going to: cmd_out: {colored(cmd_out.name, 'dark_grey')} cmd_err: {colored(cmd_err.name, 'dark_grey')}"
        )
        print(
            "------------------------------------------------------------------------"
        )

    try:
        subprocess.check_call(command, shell=True, stdout=cmd_out, stderr=cmd_err)
        if verbose:
            print(
                f'Exited command successfully ✅ -- reference:\n {colored(command, "dark_grey")}'
            )
            print(
                "======================================================================================"
            )
            print()
    except Exception as e:
        print(
            f"❌❌❌❌❌❌❌❌❌❌❌❌❌Terminated with error❌❌❌❌❌❌❌❌❌❌❌❌❌❌❌ err_log @ {colored(cmd_err.name, 'dark_grey')}"
        )
        print(colored(str(e), "red"))
        print(
            "======================================================================================"
        )
        print()
        os._exit(1)


def execute_command(
    command: str,
    *,
    dry_run: bool = False,
    verbose: bool = True,
    cmd_out: Optional[str] = None,
    cmd_err: Optional[str] = None,
):
    cmd_out = sys.stdout if cmd_out is None else open(cmd_out, "a")
    cmd_err = sys.stderr if cmd_err is None else open(cmd_err, "a")

    execute_command_helper(
        command, dry_run=dry_run, verbose=verbose, cmd_out=cmd_out, cmd_err=cmd_err
    )

    if cmd_out is not sys.stdout:
        cmd_out.close()
    if cmd_err is not sys.stderr:
        cmd_err.close()
