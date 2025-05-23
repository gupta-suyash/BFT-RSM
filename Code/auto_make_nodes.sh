#!/bin/bash
run_dr_or_ccf="$1"

GP_NAME="exp-group"
TEMPLATE="kafka-unified-5-spot" # "kafka-unified-3-spot"
RSM1_ZONE="us-west4-a"
RSM2_ZONE="us-west4-a"
KAFKA_ZONE="us-west4-a"
num_nodes_rsm_1=19
num_nodes_rsm_2=19
client=2
num_nodes_kafka=4
key_file="$HOME/.ssh/id_ed25519" # TODO: Replace with your ssh key
username="scrooge"               # TODO: Replace with your username

if [ "$run_dr_or_ccf" = "True" ]; then
    RSM1_ZONE="us-west4-a" # us-east1/2/3/4, us-south1, us-west1/2/3/4
	RSM2_ZONE="us-east5-a"
	KAFKA_ZONE="us-east5-a"
	num_nodes_rsm_1=5
	num_nodes_rsm_2=5
	client=2
fi

# Create machines
yes | gcloud beta compute instance-groups managed create "${GP_NAME}-rsm-1" --project=scrooge-398722 --base-instance-name="${GP_NAME}-rsm-1" --size="$((num_nodes_rsm_1+(client+1)/2))" --template=projects/scrooge-398722/global/instanceTemplates/${TEMPLATE} --zone="${RSM1_ZONE}" --list-managed-instances-results=PAGELESS --stateful-internal-ip=interface-name=nic0,auto-delete=never --no-force-update-on-repair --default-action-on-vm-failure=repair &
yes | gcloud beta compute instance-groups managed create "${GP_NAME}-rsm-2" --project=scrooge-398722 --base-instance-name="${GP_NAME}-rsm-2" --size="$((num_nodes_rsm_2+client/2))" --template=projects/scrooge-398722/global/instanceTemplates/${TEMPLATE} --zone="${RSM2_ZONE}" --list-managed-instances-results=PAGELESS --stateful-internal-ip=interface-name=nic0,auto-delete=never --no-force-update-on-repair --default-action-on-vm-failure=repair &
yes | gcloud beta compute instance-groups managed create "${GP_NAME}-kafka" --project=scrooge-398722 --base-instance-name="${GP_NAME}-kafka" --size="$((num_nodes_kafka))" --template=projects/scrooge-398722/global/instanceTemplates/kafka-unified-5-spot-large-disk --zone="${KAFKA_ZONE}" --list-managed-instances-results=PAGELESS --stateful-internal-ip=interface-name=nic0,auto-delete=never --no-force-update-on-repair --default-action-on-vm-failure=repair &
wait

# Wait for the groups to all become stable
gcloud compute instance-groups managed wait-until --stable "${GP_NAME}-rsm-1" --project=scrooge-398722 --zone="${RSM1_ZONE}"
gcloud compute instance-groups managed wait-until --stable "${GP_NAME}-rsm-2" --project=scrooge-398722 --zone="${RSM2_ZONE}"
gcloud compute instance-groups managed wait-until --stable "${GP_NAME}-kafka" --project=scrooge-398722 --zone="${KAFKA_ZONE}"
wait

# Get all the ip addresses of machines
gcloud compute instances list --filter="name~^${GP_NAME}-rsm-1" --format='value(networkInterfaces[0].networkIP)' > /tmp/RSM1_ips.txt &
gcloud compute instances list --filter="name~^${GP_NAME}-rsm-2" --format='value(networkInterfaces[0].networkIP)' > /tmp/RSM2_ips.txt &
gcloud compute instances list --filter="name~^${GP_NAME}-kafka" --format='value(networkInterfaces[0].networkIP)' > /tmp/KAFKA_ips.txt &
wait

echo "Successfully created all nodes."

# Populate arrays with all ip addresses
ar=($(cat /tmp/RSM1_ips.txt))
RSM1=(${ar[@]::${num_nodes_rsm_1}})
CLIENT=(${ar[@]:${num_nodes_rsm_1}}) # First client node is in RSM1
CLIENT_RSM1=(${ar[@]:${num_nodes_rsm_1}})

ar=($(cat /tmp/RSM2_ips.txt))
RSM2=(${ar[@]::${num_nodes_rsm_2}})
CLIENT+=(${ar[@]:${num_nodes_rsm_2}}) # Second client node would be in RSM2
CLIENT_RSM2=(${ar[@]:${num_nodes_rsm_2}})

ar=($(cat /tmp/KAFKA_ips.txt))
ZOOKEEPER=(${ar[@]::1})
KAFKA=(${ar[@]:1})

# Pre-Compile kafka code
git_pids=()
for i in ${!RSM2[@]}; do
	ssh -i ${key_file} -o StrictHostKeyChecking=no -t "${RSM2[$i]}" 'rm -rf tmp/output.json; cd $HOME/scrooge-kafka && git fetch && git reset --hard 4a625b192a9a03dff9ee09fe915473c087e8feee; sbt compile'	1>/dev/null  2>&1 </dev/null &
	git_pids+=($!)
done
for i in ${!RSM1[@]}; do
	ssh -i ${key_file} -o StrictHostKeyChecking=no -t "${RSM1[$i]}" 'rm -rf tmp/output.json; cd $HOME/scrooge-kafka && git fetch && git reset --hard 4a625b192a9a03dff9ee09fe915473c087e8feee; sbt compile' 1>/dev/null 2>&1 </dev/null &
	git_pids+=($!)
done

# Wait for kafka compilation to finish
for pid in ${git_pids[*]}; do
	wait $pid
done

echo "successfully pre-compiled kafka code on all nodes"
