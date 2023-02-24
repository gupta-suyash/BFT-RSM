### Natacha Crooks - 2014, Micah Murray - 2022
### Contains utility functions related to
### dealing with property files
############################################

import json
import os
import subprocess
from pprint import pprint

## Returns property file as
## a python dictionary.
## Throws an exception if
## file does note exist and returns
## empty dict
def loadJsonFile(jsonFileName):
    data = {}
    try:
        json_data = open(jsonFileName)
    except:
        print("File does not exist " + jsonFileName)
    try:
        data = json.load(json_data)
    except:
        print("Incorrect JSON Format  " + jsonFileName)
        raise
    return data

## Prints Content of Json file
def printPropertyFile(jsonFileName):
    try:
        json_data = open(jsonFileName)
        data = json.loads(json_data)
        pprint(data)
        json_data.close()
    except:
        print("File does not exist")

## Get GIT Revision Hash
def getGitHash(projectpath):
    currentdir = os.getcwd();
    os.chdir(projectpath)
    hash = subprocess.check_output(['git', 'rev-parse', 'HEAD'])
    os.chdir(currentdir)
    return hash
