#ifndef _GLOBAL_H_
#define _GLOBAL_H_

#include <iostream>
#include <vector>
#include <memory>
#include "types.h"
#include "../configuration/config.h"

using namespace std;

class Pipeline;
class PipeQueue;


//// Constants
//#ifndef CONSTANTS
//
//#endif

// List of global variables and configuration parameters.

extern UInt16 g_thread_cnt;
extern UInt16 g_num_rsm;
extern UInt16 g_nodes_rsm;
extern UInt16 g_node_cnt;
extern UInt16 g_node_id;
extern UInt16 g_rsm_id; // RSM Id for this node.
extern UInt16 g_max_fail; 

extern UInt16 g_port_num;

// Pointer to sender queue
extern unique_ptr<PipeQueue> sp_queue;
extern PipeQueue *sp_qptr;

// Pointer to pipeline
extern unique_ptr<Pipeline> pipe_obj;
extern Pipeline *pipe_ptr;
			
UInt16 get_num_of_rsm();
UInt16 get_nodes_rsm();

UInt16 get_node_id();
void set_node_id(UInt16 nid);

UInt16 get_rsm_id();
void set_rsm_id(UInt16 rsm_id);

UInt16 get_port_num();

UInt16 get_max_nodes_fail();	


enum MessageType {
	kPipeQueue=0
};


#endif
