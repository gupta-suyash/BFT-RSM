run_dr_or_ccf="$1"

GP_NAME="exp_group"
TEMPLATE="kafka-unified-5-spot" # "kafka-unified-3-spot"
RSM1_ZONE="us-west4-a"
RSM2_ZONE="us-west4-a"
KAFKA_ZONE="us-west4-a"


if [ "$run_dr_or_ccf" = "true" ]; then
    RSM1_ZONE="us-west4-a" # us-east1/2/3/4, us-south1, us-west1/2/3/4
	RSM2_ZONE="us-east5-a"
	KAFKA_ZONE="us-east5-a"
fi

yes | gcloud compute instance-groups managed delete "${GP_NAME}-rsm-1" --zone $RSM1_ZONE &
yes | gcloud compute instance-groups managed delete "${GP_NAME}-rsm-2" --zone $RSM2_ZONE &
yes | gcloud compute instance-groups managed delete "${GP_NAME}-kafka" --zone $KAFKA_ZONE &
wait