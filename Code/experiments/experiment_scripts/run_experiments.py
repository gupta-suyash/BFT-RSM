import sys
import concurrent.futures

from scale_clients import *

def main():
    if len(sys.argv) != 2:
        sys.stderr.write('Usage: python3 %s <config_file>\n' % sys.argv[0])
        sys.exit(1)

    with concurrent.futures.ThreadPoolExecutor(max_workers=16) as executor:
        setup(sys.argv[1]) 
        run(sys.argv[1]) # FINISH UPDATING


if __name__ == "__main__":
    main()
