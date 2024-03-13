#!/bin/bash

rm /tmp/all_ips.txt
num_ips_read=0
while ((${num_ips_read} < $((num_nodes_rsm_1+num_nodes_rsm_2+client)))); do
	        gcloud compute instances list --filter="name~^${GP_NAME}" --format='value(networkInterfaces[0].networkIP)' > /tmp/all_ips.txt        output=$(cat /tmp/all_ips.txt)
		        ar=($output)
			        num_ips_read="${#ar[@]}"
			done
			# && sudo wondershaper ens4 2000000 2000000'
			parallel -v --jobs=0 ssh -o StrictHostKeyChecking=no -t {1} 'sudo wondershaper clear' ::: "${ar[@]:0:19}";
			parallel -v --jobs=0 ssh -o StrictHostKeyChecking=no -t {1} 'sudo wondershaper clear' ::: "${ar[@]:19:19}";
