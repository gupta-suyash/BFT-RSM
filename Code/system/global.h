#pragma once

#include "../configuration/config.h"
#include "crosschainmessage.pb.h"
#include <boost/lockfree/queue.hpp>
#include <cstring>
#include <iostream>
#include <memory>
#include <queue>
#include <utility>
#include <vector>

using namespace std;

class Pipeline;
class PipeQueue;
// class ProtoMessage;
class Acknowledgment;

// List of global variables and configuration parameters.

extern uint16_t g_thread_cnt;
extern uint16_t g_num_rsm;
extern uint16_t g_nodes_rsm;
extern uint16_t g_node_cnt;
extern uint16_t g_node_id;
extern uint16_t g_node_rsm_id;
extern uint16_t g_rsm_id;       // RSM Id for this node.
extern uint16_t g_other_rsm_id; // RSM Id of other RSM.
extern uint16_t g_max_fail;

extern uint16_t g_port_num;

// Pointer to sender queue
extern unique_ptr<PipeQueue> sp_queue;
extern PipeQueue *sp_qptr;

// Pointer to pipeline
extern unique_ptr<Pipeline> pipe_obj;
extern Pipeline *pipe_ptr;

uint16_t get_num_of_rsm();
uint16_t get_nodes_rsm();

uint16_t get_node_id();
void set_node_id(uint16_t nid);

uint16_t get_rsm_id();
void set_rsm_id(uint16_t rsm_id);

uint16_t get_other_rsm_id();
void set_other_rsm_id(uint16_t rsm_id);

uint16_t get_node_rsm_id();
void set_node_rsm_id(uint16_t nid);

uint16_t get_port_num();

uint16_t get_max_nodes_fail();

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
extern std::queue<crosschain_proto::CrossChainMessage> in_queue;

// Object to access the Acknowledgments.
extern Acknowledgment *ack_obj;
