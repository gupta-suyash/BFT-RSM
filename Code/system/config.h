#ifndef _CONFIG_H_
#define _CONFIG_H_
#define OWN_RSM_SIZE 19
#define OTHER_RSM_SIZE 19
#define OWN_RSM_MAX_NODES_FAIL 6
#define OTHER_RSM_MAX_NODES_FAIL 6
#define NUMBER_PACKETS 10000
#define PACKET_SIZE 1000000
#define NETWORK_DIR "/home/scrooge/BFT-RSM/Code/configuration/"
#define LOG_DIR "/home/scrooge/BFT-RSM/Code/experiments/results/"
#define USE_DEBUG_LOGS_BOOL false
#define WARMUP_TIME 10s
#define TOTAL_TIME 80s
#define BATCH_SIZE 200000
#define BATCH_CREATION_TIME 1ms
#define MAX_NNG_BLOCKING_TIME 500ms
#define PIPELINE_BUFFER_SIZE 8
#define MESSAGE_BUFFER_SIZE 256
#define KLIST_SIZE 64
#define SCROOGE false
#define ALL_TO_ALL true
#define ONE_TO_ONE false
#define GEOBFT false
#define LEADER false
#define FILE_RSM true
#define NOOP_DELAY 1ms
#define MAX_MESSAGE_DELAY 100ms
#define QUACK_WINDOW 1000
#define ACK_WINDOW 100
#endif

