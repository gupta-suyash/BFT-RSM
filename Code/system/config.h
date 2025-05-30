#ifndef _CONFIG_H_
#define _CONFIG_H_
#define OWN_RSM_SIZE 5
#define OTHER_RSM_SIZE 5
#define OWN_RSM_MAX_NODES_FAIL 2
#define OTHER_RSM_MAX_NODES_FAIL 2
#define NUMBER_PACKETS 10000
#define PACKET_SIZE 14304
#define NETWORK_DIR "/home/scrooge/BFT-RSM/Code/configuration/"
#define LOG_DIR "/home/scrooge/BFT-RSM/Code/experiments/results/"
#define USE_DEBUG_LOGS_BOOL false
#define WARMUP_TIME 45s
#define TOTAL_TIME 75s
#define BATCH_SIZE 200000
#define BATCH_CREATION_TIME 1ms
#define MAX_NNG_BLOCKING_TIME 500ms
#define PIPELINE_BUFFER_SIZE 8
#define MESSAGE_BUFFER_SIZE 5000
#define KLIST_SIZE 64
#define SCROOGE false
#define ALL_TO_ALL true
#define ONE_TO_ONE false
#define GEOBFT false
#define LEADER false
#define FILE_RSM false
#define NOOP_DELAY 5ms
#define MAX_MESSAGE_DELAY 1ms
#define QUACK_WINDOW 1048576
#define ACK_WINDOW 1048576
#define WRITE_DR false
#define WRITE_CCF true
#define BYZ_MODE "NO"
#define SIMULATE_CRASH false
#define THROTTLE_FILE false
#endif

