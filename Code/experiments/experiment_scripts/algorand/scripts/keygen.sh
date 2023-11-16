# !/bin/bash
while getopts p:a: flag
do
	case "${flag}" in
		p) pathname=${OPTARG};;
		a) addr=${OPTARG};;
	esac
dummy_node
export ALGORAND_DATA=$pathname/node/
echo $ALGORAND_DATA
~/go/bin/goal kmd start
~/go/bin/goal node start
~/go/bin/goal account addpartkey -a $addr --roundFirstValid=1 --roundLastValid=6000000  --keyDilution=10000
~/go/bin/goal account partkeyinfo > $ALGORAND_DATA/partkeyinfo.json
~/go/bin/goal kmd stop
~/go/bin/goal node stop
