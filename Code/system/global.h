#ifndef _GLOBAL_H_
#define _GLOBAL_H_

#include <iostream>
#include "types.h"
#include "../configuration/config.h"

using namespace std;

// List of global variables and configuration parameters.

extern UInt16 g_thread_cnt;
extern UInt16 g_num_rsm;
extern UInt16 g_nodes_rsm;
extern UInt16 g_node_cnt;
extern UInt16 g_node_id;
extern UInt16 g_rsm_id; // RSM Id for this node.
			

UInt16 get_nodes_rsm();

UInt16 get_node_id();
void set_node_id(UInt16 nid);

UInt16 get_rsm_id();
void set_rsm_id(UInt16 rsm_id);


#endif
