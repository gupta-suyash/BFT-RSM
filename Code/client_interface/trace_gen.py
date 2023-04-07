#!/usr/bin/env python3
import string
import random

def get_random_string(length):
    # choose from all lowercase letter
    letters = string.ascii_lowercase
    result_str = ''.join(random.choice(letters) for i in range(length))
    result_str += '\n'
    return result_str
    #print("Random string of length", length, "is:", result_str)

def main():
    # open a file
    print("Test: ", get_random_string(1000))
    filename = "trace_" + str(1000) + ".txt"
    f = open(filename, "a")
    for i in range(0, 100):
        f.write(get_random_string(1000))
    f.close()
    print("Trace creation complete!");

if __name__ == "__main__":
    main()
