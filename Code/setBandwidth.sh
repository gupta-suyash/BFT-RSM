#!/bin/bash

exec_dir="/home/scrooge"

# Setting size of the RSMs.
rsm1_sz=$(sudo cat ${exec_dir}/network0_bw.txt | wc -l)
rsm2_sz=$(sudo cat ${exec_dir}/network1_bw.txt | wc -l)
echo "RSM Sizes: ${rsm1_sz} :: ${rsm2_sz}"

# Setting the RSMs
#echo "Setting RSM arrays"

#RSM1
output=$(cat ${exec_dir}/net0urls.txt)   #RSM1 information is in net0urls.txt
bw=($output)
rsm1=(${bw[@]::${rsm1_sz}})

#RSM2
output=$(cat ${exec_dir}/net1urls.txt)   #Bandwidth information is in net1urls.txt
bw=($output)
rsm2=(${bw[@]::${rsm2_sz}})
#echo "RCHECK1: ${rsm1_sz} :: ${rsm2_sz}"

# Setting Bandwidth for Stake Experiments.
#echo "Setting Node bandwidths"

#RSM1
output=$(cat ${exec_dir}/network0_bw.txt)   #Bandwidth information is in network0_bw.txt
bw=($output)
rsm1_bw=(${bw[@]::${rsm1_sz}})

#RSM2
output=$(cat ${exec_dir}/network1_bw.txt)   #Bandwidth information is in network1_bw.txt
bw=($output)
rsm2_bw=(${bw[@]::${rsm2_sz}})

#echo "RCHECK2: ${rsm1_sz} :: ${rsm2_sz}"

# Bandwidth modification happens next.
NETCARD=ens4
MAXBANDWIDTH=20452173

# Confirming ssh node
hostname -I

# reinit
tc qdisc del dev $NETCARD root handle 1
tc qdisc add dev $NETCARD root handle 1: htb default 9999

# create the default class
tc class add dev $NETCARD parent 1:0 classid 1:9999 htb rate $(( $MAXBANDWIDTH ))kbit ceil $(( $MAXBANDWIDTH ))kbit burst 5k prio 9999

# control bandwidth per IP
declare -A ipctrl
mark=0

# define list of IP and bandwidth (in kilo bits per seconds) below
# For RSM1.
icount=0
echo "CHECK: ${icount} :: ${rsm1_sz}"
while [[ ${icount} < ${rsm1_sz} ]] 
do
    #echo -n "HAPPY: ${rsm1[$icount]} "
    #echo "${rsm1_bw[$icount]}"

    ip=${rsm1[$icount]}
    mark=$(( mark + 1 ))
    bandwidth="${rsm1_bw[$icount]}"

    # traffic shaping rule
    tc class add dev $NETCARD parent 1:0 classid 1:$mark htb rate $(( $bandwidth ))kbit ceil $(( $bandwidth ))kbit burst 5k prio $mark

    # netfilter packet marking rule
    iptables -t mangle -A INPUT -i $NETCARD -s $ip -j CONNMARK --set-mark $mark

    # filter that bind the two
    tc filter add dev $NETCARD parent 1:0 protocol ip prio $mark handle $mark fw flowid 1:$mark

    echo "IP $ip is attached to mark $mark and limited to $bandwidth kbps"

    icount=$((icount+1))
done

# For RSM2.
icount=0
while [[ ${icount} -lt ${rsm2_sz} ]]
do
    #echo -n "NAPPY: ${rsm2[$icount]} "
    #echo "${rsm2_bw[$icount]}"

    ip=${rsm2[$icount]}
    mark=$(( mark + 1 ))
    bandwidth="${rsm2_bw[$icount]}"

    # traffic shaping rule
    tc class add dev $NETCARD parent 1:0 classid 1:$mark htb rate $(( $bandwidth ))kbit ceil $(( $bandwidth ))kbit burst 5k prio $mark

    # netfilter packet marking rule
    iptables -t mangle -A INPUT -i $NETCARD -s $ip -j CONNMARK --set-mark $mark

    # filter that bind the two
    tc filter add dev $NETCARD parent 1:0 protocol ip prio $mark handle $mark fw flowid 1:$mark

    echo "IP $ip is attached to mark $mark and limited to $bandwidth kbps"

    icount=$((icount + 1))
done


#mark=0
#for ip in "${!ipctrl[@]}"
#do
#    mark=$(( mark + 1 ))
#    bandwidth=${ipctrl[$ip]}
#
#    # traffic shaping rule
#    tc class add dev $NETCARD parent 1:0 classid 1:$mark htb rate $(( $bandwidth ))kbit ceil $(( $bandwidth ))kbit burst 5k prio $mark
#
#    # netfilter packet marking rule
#    iptables -t mangle -A INPUT -i $NETCARD -s $ip -j CONNMARK --set-mark $mark
#
#    # filter that bind the two
#    tc filter add dev $NETCARD parent 1:0 protocol ip prio $mark handle $mark fw flowid 1:$mark
#
#    echo "IP $ip is attached to mark $mark and limited to $bandwidth kbps"
#done

#propagate netfilter marks on connections
iptables -t mangle -A POSTROUTING -j CONNMARK --restore-mark

