#pragma once

#include "../configuration/config.h"
#include "scrooge_message.pb.h"
#include <boost/lockfree/queue.hpp>
#include <chrono>
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
using namespace std::chrono_literals;

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
extern PipeQueue *sp_qptr;

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

// Object to access the Acknowledgments.
extern Acknowledgment *ack_obj;
