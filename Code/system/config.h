#ifndef _CONFIG_H_
#define _CONFIG_H_
#define OWN_RSM_SIZE 4
#define OTHER_RSM_SIZE 4
#define OWN_RSM_MAX_NODES_FAIL 1
#define OTHER_RSM_MAX_NODES_FAIL 1
#define NUMBER_PACKETS 10000
#define PACKET_SIZE 10000
#define NETWORK_DIR "/proj/ove-PG0/reggie/BFT-RSM/Code/configuration/"
#define LOG_DIR "/proj/ove-PG0/reggie/BFT-RSM/Code/experiments/results/"
#define USE_DEBUG_LOGS_BOOL false
#define WARMUP_TIME 10s
#define TOTAL_TIME 30s
#define BATCH_SIZE 26214
#define BATCH_CREATION_TIME 1ms
#define MAX_NNG_BLOCKING_TIME 500ms
#define PIPELINE_BUFFER_SIZE 8
#define MESSAGE_BUFFER_SIZE 256
#define KLIST_SIZE 64
#define SCROOGE true
#define ALL_TO_ALL false
#define ONE_TO_ONE false
#define FILE_RSM true
#endif
