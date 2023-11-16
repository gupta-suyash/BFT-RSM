#!/usr/bin/env python3
import sys
import os
import subprocess

def main():
    subprocess.check_call(['. ./simple.sh', "hi", "hola", "bonjour", "salut"],shell=True, stdout=sys.stdout, stderr=sys.stdout)


if __name__ == "__main__":
        main()
