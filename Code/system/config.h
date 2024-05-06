#ifndef _CONFIG_H_
#define _CONFIG_H_
#define OWN_RSM_SIZE 16
#define OTHER_RSM_SIZE 16
#define OWN_RSM_MAX_NODES_FAIL 5
#define OTHER_RSM_MAX_NODES_FAIL 5
#define NUMBER_PACKETS 10000
#define PACKET_SIZE 1000000
#define NETWORK_DIR "/home/scrooge/BFT-RSM/Code/configuration/"
#define LOG_DIR "/home/scrooge/BFT-RSM/Code/experiments/results/"
#define USE_DEBUG_LOGS_BOOL false
#define WARMUP_TIME 10s
#define TOTAL_TIME 40s
#define BATCH_SIZE 200000
#define BATCH_CREATION_TIME 1ms
#define MAX_NNG_BLOCKING_TIME 10ms
#define PIPELINE_BUFFER_SIZE 256
#define MESSAGE_BUFFER_SIZE 256
#define KLIST_SIZE 0
#define SCROOGE true
#define ALL_TO_ALL false
#define ONE_TO_ONE false
#define GEOBFT false
#define LEADER false
#define FILE_RSM true
#define NOOP_DELAY 4ms
#define MAX_MESSAGE_DELAY 75ms
#define QUACK_WINDOW 2500
#define ACK_WINDOW 20
#endif

