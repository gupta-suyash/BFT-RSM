#include "global.h"
#include "../configuration/config.h"

uint16_t g_thread_cnt = THREAD_CNT;
uint64_t g_num_rsm = NUM_RSM;
uint64_t g_nodes_rsm = NODES_RSM;
uint64_t g_nodes_other_rsm = NODES_RSM;
uint64_t g_node_cnt = NUM_RSM * NODES_RSM;
uint64_t g_node_id = 0;
uint64_t g_node_rsm_id = 0;
uint64_t g_other_rsm_id; // RSM Id of other RSM.
uint64_t g_rsm_id = 0;
uint64_t g_max_fail = 1; // MAX_NODES_FAIL;
uint64_t g_max_fail_other_rsm = 1;
uint64_t g_number_of_packets = 200;
uint16_t g_port_num = PORT_NUM;

PipeQueue *sp_qptr;

// Pointer to pipeline
Pipeline *pipe_ptr;

// Input queue.
std::queue<scrooge::CrossChainMessage> in_queue;

// Acknowledgement object.
Acknowledgment *ack_obj;

/* Get the total number of RSMs.
 *
 * @return g_num_rsm.
 */
uint64_t get_num_of_rsm()
{
    return g_num_rsm;
}

/* Get the number of nodes in a RSM.
 * At present, we assume each RSM has same number of nodes.
 *
 * @return g_nodes_rsm.
 */
uint64_t get_nodes_rsm()
{
    return g_nodes_rsm;
}

uint64_t get_nodes_other_rsm()
{
    return g_nodes_other_rsm;
}

void set_num_of_nodes_rsm(bool thisNodeRsm, uint64_t num_nodes_rsm)
{
    if (thisNodeRsm) {
        g_nodes_rsm = num_nodes_rsm;
        return;
    }
    g_nodes_other_rsm = num_nodes_rsm;

}

/* Get the node's id.
 *
 * @return g_node_id.
 */
uint64_t get_node_id()
{
    return g_node_id;
}

/* Set the node'd id.
 *
 * @param nid is the node id.
 */
void set_node_id(uint64_t nid)
{
    g_node_id = nid;
}

/* Get the id of the RSM this node belongs.
 *
 * @return g_rsm_id.
 */
uint64_t get_rsm_id()
{
    return g_rsm_id;
}

/* Set the id of the RSM this node belongs.
 *
 * @param rsm_id is the RSM id.
 */
void set_rsm_id(uint64_t rsm_id)
{
    g_rsm_id = rsm_id;
}

/* Get the id of the other RSM.
 *
 * @return g_other_rsm_id.
 */
uint64_t get_other_rsm_id()
{
    return g_other_rsm_id;
}

/* Set the id of the other RSM.
 *
 * @param rsm_id is the RSM id.
 */
void set_other_rsm_id(uint64_t rsm_id)
{
    g_other_rsm_id = rsm_id;
}

/* Get the node's RSM id.
 *
 * @return g_node_rsm_id.
 */
uint64_t get_node_rsm_id()
{
    return g_node_rsm_id;
}

/* Set this node's id w.r.t its RSM.
 *
 * @param nid is the node's RSM id.
 */
void set_node_rsm_id(uint64_t nid)
{
    g_node_rsm_id = nid;
}

/* Get the starting port number.
 *
 * @return g_port_num.
 */
uint16_t get_port_num()
{
    return g_port_num;
}

/* Get the maximum number of nodes that can fail.
 *
 * @return g_max_fail.
 */
uint64_t get_max_nodes_fail(bool thisNodeRsm)
{
    return thisNodeRsm ? g_max_fail : g_max_fail_other_rsm;
}

void set_max_nodes_fail(bool thisNodeRsm, uint64_t max_nodes_fail)
{
    if (thisNodeRsm) {
        g_max_fail = max_nodes_fail;
        return;
    }
    g_max_fail_other_rsm = max_nodes_fail;

}

uint64_t get_number_of_packets()
{
    return g_number_of_packets;
}

void set_number_of_packets(uint64_t packet_number)
{
    g_number_of_packets = packet_number;
}
