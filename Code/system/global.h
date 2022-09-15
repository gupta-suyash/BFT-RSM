#ifndef _GLOBAL_H_
#define _GLOBAL_H_

#include <iostream>
#include <vector>
#include <memory>
#include <cstring>
#include <utility>
#include "types.h"
#include "../configuration/config.h"

#include <boost/lockfree/queue.hpp>

using namespace std;

class Pipeline;
class PipeQueue;
class ProtoMessage;
class Acknowledgment;


// List of global variables and configuration parameters.

extern UInt16 g_thread_cnt;
extern UInt16 g_num_rsm;
extern UInt16 g_nodes_rsm;
extern UInt16 g_node_cnt;
extern UInt16 g_node_id;
extern UInt16 g_node_rsm_id;
extern UInt16 g_rsm_id; // RSM Id for this node.
extern UInt16 g_other_rsm_id; // RSM Id of other RSM.
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

UInt16 get_other_rsm_id();
void set_other_rsm_id(UInt16 rsm_id);

UInt16 get_node_rsm_id();
void set_node_rsm_id(UInt16 nid);

UInt16 get_port_num();

UInt16 get_max_nodes_fail();	


// An enum that states different types of messages.
enum MessageType {
	kSend=0,
	kForward
};


// Copy data in buffer d starting at position p to v.
#define COPY_VAL(v, d, p)		\
	memcpy(&v, &d[p], sizeof(v));	\
	p += sizeof(v);

// Copy data at v to buffer d starting from position p.
#define COPY_BUF(d, v, p)		\
	memcpy(&((char *)d)[p], (char *)&v, sizeof(v)); \
	p += sizeof(v);


// Queue to interact with the protocol accessing Scrooge.
extern boost::lockfree::queue<ProtoMessage *> *in_queue;

// Object to access the Acknowledgments.
extern Acknowledgment *ack_obj;

#endif
