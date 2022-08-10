#include "global.h"
#include "../configuration/config.h"

UInt16 g_thread_cnt = THREAD_CNT;
UInt16 g_num_rsm = NUM_RSM;
UInt16 g_nodes_rsm = NODES_RSM;
UInt16 g_node_cnt = NUM_RSM * NODES_RSM;
UInt16 g_node_id = 0;
UInt16 g_rsm_id = 0;


/* Get the number of nodes in a RSM.
 *
 * @return g_nodes_rsm..
 */ 
UInt16 get_nodes_rsm()
{
	return g_nodes_rsm;
}

/* Get the node's id.
 *
 * @return g_node_id.
 */ 
UInt16 get_node_id()
{
	return g_node_id;
}

/* Set the node'd id.
 *
 * @param nid is the node id.
 */ 
void set_node_id(UInt16 nid) 
{
	g_node_id = nid;
}

/* Get the id of the RSM this node belongs.
 *
 * @return g_rsm_id.
 */ 
UInt16 get_rsm_id()
{
	return g_rsm_id;
}

/* Set the id of the RSM this node belongs.
 *
 * @param rsm_id is the RSM id.
 */ 
void set_rsm_id(UInt16 rsm_id) 
{
	g_rsm_id = rsm_id;
}
