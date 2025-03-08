#ifndef _CONFIG_H_
#define _CONFIG_H_
#define OWN_RSM_SIZE 4
#define OTHER_RSM_SIZE 4
#define OWN_RSM_MAX_NODES_FAIL 1
#define OTHER_RSM_MAX_NODES_FAIL 1
#define NUMBER_PACKETS 10000
#define PACKET_SIZE 100
#define NETWORK_DIR "/home/scrooge/BFT-RSM/Code/configuration/"
#define LOG_DIR "/home/scrooge/BFT-RSM/Code/experiments/results/"
#define USE_DEBUG_LOGS_BOOL false
#define WARMUP_TIME 15s
#define TOTAL_TIME 30s
#define BATCH_SIZE 150000
#define BATCH_CREATION_TIME .1ms
#define MAX_NNG_BLOCKING_TIME 500ms
#define PIPELINE_BUFFER_SIZE 4
#define MESSAGE_BUFFER_SIZE 5000
#define KLIST_SIZE 64
#define SCROOGE true
#define ALL_TO_ALL false
#define ONE_TO_ONE false
#define GEOBFT false
#define LEADER false
#define FILE_RSM true
#define NOOP_DELAY 5ms
#define MAX_MESSAGE_DELAY .5ms
#define QUACK_WINDOW 75000
#define ACK_WINDOW 75000
#define WRITE_DR false
#define WRITE_CCF false
#endif

