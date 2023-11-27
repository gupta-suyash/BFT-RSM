# !/bin/bash
echo "############# START KEYGEN SCRIPT"
#while getopts p:a: flag
#do
#	case "${flag}" in
#		p) pathname=${OPTARG};;
#		a) addr=${OPTARG};;
#	esac
echo "START ARGS"
echo $0
echo $1
echo "END ARGS"
export ALGORAND_DATA=$0/node/
echo $ALGORAND_DATA
cd $ALGORAND_DATA
~/go/bin/goal kmd start
~/go/bin/goal node start
~/go/bin/goal account addpartkey -a $1 --roundFirstValid=1 --roundLastValid=6000000  --keyDilution=10000
~/go/bin/goal account partkeyinfo > $ALGORAND_DATA/partkeyinfo.json
~/go/bin/goal kmd stop
~/go/bin/goal node stop
echo "############# END KEYGEN SCRIPT"
