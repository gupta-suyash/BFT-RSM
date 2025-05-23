#!/usr/bin/env python3
import sys
import concurrent.futures

from experiment_fxns import *

def main():
    if len(sys.argv) != 3:
        sys.stderr.write('Usage: python3 %s <config_file> <experiment name>\n' % sys.argv[0])
        sys.exit(1)

    expDir = setup(sys.argv[1], sys.argv[2])
    run(sys.argv[1], sys.argv[2], expDir)
    print("Experiment completed successfully.")
        


if __name__ == "__main__":
    main()
