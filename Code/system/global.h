#pragma once

#include "../configuration/config.h"
#include "scrooge_message.pb.h"
#include <boost/lockfree/queue.hpp>
#include <cstring>
#include <iostream>
#include <memory>
#include <queue>
#include <utility>
#include <vector>

// Enable all spdlog logging macros for development
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#include <spdlog/spdlog.h>

using namespace std;

class Pipeline;
class PipeQueue;
// class ProtoMessage;
class Acknowledgment;

// List of global variables and configuration parameters.

extern uint16_t g_thread_cnt;
extern uint64_t g_num_rsm;
extern uint64_t g_nodes_rsm;
extern uint64_t g_nodes_other_rsm;
extern uint64_t g_node_cnt;
extern uint64_t g_node_id;
extern uint64_t g_node_rsm_id;
extern uint64_t g_rsm_id;       // RSM Id for this node.
extern uint64_t g_other_rsm_id; // RSM Id of other RSM.
extern uint64_t g_max_fail_rsm;
extern uint64_t g_max_fail_other_rsm;
extern uint64_t g_number_of_packets;
extern uint64_t g_packet_size;
extern uint16_t g_port_num;

// Pointer to sender queue
extern unique_ptr<PipeQueue> sp_queue;
extern PipeQueue *sp_qptr;

// Pointer to pipeline
extern unique_ptr<Pipeline> pipe_obj;
extern Pipeline *pipe_ptr;

uint64_t get_num_of_rsm();

uint64_t get_nodes_rsm();
uint64_t get_nodes_other_rsm();
void set_num_of_nodes_rsm(bool thisNodeRsm, uint64_t num_nodes_rsm);

uint64_t get_node_id();
void set_node_id(uint64_t nid);

uint64_t get_rsm_id();
void set_rsm_id(uint64_t rsm_id);

uint64_t get_other_rsm_id();
void set_other_rsm_id(uint64_t rsm_id);

uint64_t get_node_rsm_id();
void set_node_rsm_id(uint64_t nid);

uint16_t get_port_num();

uint64_t get_max_nodes_fail(bool thisNodeRsm);
void set_max_nodes_fail(bool thisNodeRsm, uint64_t max_nodes_fail);

uint64_t get_number_of_packets();
void set_number_of_packets(uint64_t packet_number);

uint64_t get_packet_size();
void set_packet_size(uint64_t packet_size);

// An enum that states different types of messages.
enum MessageType
{
    kSend = 0,
    kForward
};

// Copy data in buffer d starting at position p to v.
#define COPY_VAL(v, d, p)                                                                                              \
    memcpy(&v, &d[p], sizeof(v));                                                                                      \
    p += sizeof(v);

// Copy data at v to buffer d starting from position p.
#define COPY_BUF(d, v, p)                                                                                              \
    memcpy(&((char *)d)[p], (char *)&v, sizeof(v));                                                                    \
    p += sizeof(v);

// Queue to interact with the protocol accessing Scrooge.
// extern boost::lockfree::queue<ProtoMessage *> *in_queue;
extern std::queue<scrooge::CrossChainMessage> in_queue;

// Object to access the Acknowledgments.
extern Acknowledgment *ack_obj;
