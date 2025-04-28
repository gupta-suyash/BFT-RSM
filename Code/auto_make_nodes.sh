
run_dr_or_ccf="$1"

GP_NAME="exp_group"
TEMPLATE="kafka-unified-5-spot" # "kafka-unified-3-spot"
RSM1_ZONE="us-west4-a"
RSM2_ZONE="us-west4-a"
KAFKA_ZONE="us-west4-a"
num_nodes="19"


if [ "$run_dr_or_ccf" = "true" ]; then
    RSM1_ZONE="us-west4-a" # us-east1/2/3/4, us-south1, us-west1/2/3/4
	RSM2_ZONE="us-east5-a"
	KAFKA_ZONE="us-east5-a"
	num_nodes="5"
fi

# Create machines
yes | gcloud beta compute instance-groups managed create "${GP_NAME}-rsm-1" --project=scrooge-398722 --base-instance-name="${GP_NAME}-rsm-1" --size="${num_nodes}" --template=projects/scrooge-398722/global/instanceTemplates/${TEMPLATE} --zone="${RSM1_ZONE}" --list-managed-instances-results=PAGELESS --stateful-internal-ip=interface-name=nic0,auto-delete=never --no-force-update-on-repair --default-action-on-vm-failure=repair &
yes | gcloud beta compute instance-groups managed create "${GP_NAME}-rsm-2" --project=scrooge-398722 --base-instance-name="${GP_NAME}-rsm-2" --size="${num_nodes}" --template=projects/scrooge-398722/global/instanceTemplates/${TEMPLATE} --zone="${RSM2_ZONE}" --list-managed-instances-results=PAGELESS --stateful-internal-ip=interface-name=nic0,auto-delete=never --no-force-update-on-repair --default-action-on-vm-failure=repair &
yes | gcloud beta compute instance-groups managed create "${GP_NAME}-kafka" --project=scrooge-398722 --base-instance-name="${GP_NAME}-kafka" --size="4" --template=projects/scrooge-398722/global/instanceTemplates/${TEMPLATE} --zone="${KAFKA_ZONE}" --list-managed-instances-results=PAGELESS --stateful-internal-ip=interface-name=nic0,auto-delete=never --no-force-update-on-repair --default-action-on-vm-failure=repair &
wait

# Wait for the groups to all become stable
gcloud compute instance-groups managed wait-until --stable "${GP_NAME}-rsm-1"
gcloud compute instance-groups managed wait-until --stable "${GP_NAME}-rsm-2"
gcloud compute instance-groups managed wait-until --stable "${GP_NAME}-kafka"
wait

# Get all the ip addresses of machines
gcloud compute instances list --filter="name~^${GP_NAME}-rsm-1" --format='value(networkInterfaces[0].networkIP)' > /tmp/RSM1_ips.txt &
gcloud compute instances list --filter="name~^${GP_NAME}-rsm-2" --format='value(networkInterfaces[0].networkIP)' > /tmp/RSM2_ips.txt &
gcloud compute instances list --filter="name~^${GP_NAME}-kafka" --format='value(networkInterfaces[0].networkIP)' > /tmp/KAFKA_ips.txt &
wait

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
local git_pids=()
for i in ${!RSM2[@]}; do
	ssh -i ${key_file} -o StrictHostKeyChecking=no -t "${RSM2[$i]}" 'rm -rf tmp/output.json; cd $HOME/scrooge-kafka && git fetch && git reset --hard ced9649b79d4fe3e4f4dc461f7b6c365f9b5233a; sbt compile' 1>/dev/null </dev/null &
	git_pids+=($!)
done
for i in ${!RSM1[@]}; do
	ssh -i ${key_file} -o StrictHostKeyChecking=no -t "${RSM1[$i]}" 'rm -rf tmp/output.json; cd $HOME/scrooge-kafka && git fetch && git reset --hard ced9649b79d4fe3e4f4dc461f7b6c365f9b5233a; sbt compile' 1>/dev/null </dev/null &
	git_pids+=($!)
done

# Wait for kafka compilation to finish
for pid in ${git_pids[*]}; do
	wait $pid
done