# !/bin/bash
NODE=/proj/ove-PG0/therealmurray/node/dummy_node

while getopts i:j:a: flag
do
	case "${flag}" in
		i) a=${OPTARG};;
		j) b=${OPTARG};;
		a) addr=${OPTARG};;
	esac
done
echo $a
export ALGORAND_DATA=$NODE/n$a$b
echo $ALGORAND_DATA
~/go/bin/goal kmd start
~/go/bin/goal node start
~/go/bin/goal account addpartkey -a $addr --roundFirstValid=1 --roundLastValid=6000000  --keyDilution=10000
~/go/bin/goal account partkeyinfo > $ALGORAND_DATA/partkeyinfo.json
~/go/bin/goal kmd stop
~/go/bin/goal node stop
