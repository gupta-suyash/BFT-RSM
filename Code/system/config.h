#ifndef _CONFIG_H_
#define _CONFIG_H_
#define OWN_RSM_SIZE 4
#define OTHER_RSM_SIZE 4
#define OWN_RSM_MAX_NODES_FAIL 1
#define OTHER_RSM_MAX_NODES_FAIL 1
#define NUMBER_PACKETS 10000
#define PACKET_SIZE 100
#define NETWORK_DIR "/proj/ove-PG0/suyash2/BFT-RSM/Code/configuration/"
#define LOG_DIR "/proj/ove-PG0/suyash2/BFT-RSM/Code/experiments/results/"
#define WARMUP_TIME 20s
#define TOTAL_TIME 60s
#define BATCH_SIZE 262144
#define BATCH_CREATION_TIME 4800us
#define MAX_NNG_BLOCKING_TIME 500ms
#define PIPELINE_BUFFER_SIZE 2048
#define MESSAGE_BUFFER_SIZE 256
#define KLIST_SIZE 5120
#endif
