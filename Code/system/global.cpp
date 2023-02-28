#include "global.h"

// List of global variables and configuration parameters.
static uint64_t g_rsm_id{};       // RSM Id for this node.
static uint64_t g_other_rsm_id{}; // RSM Id of other RSM.
static uint64_t g_number_of_packets{};
static uint64_t g_packet_size{};

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

uint64_t get_number_of_packets()
{
    return g_number_of_packets;
}

void set_number_of_packets(uint64_t packet_number)
{
    g_number_of_packets = packet_number;
}

uint64_t get_packet_size()
{
    return g_packet_size;
}

void set_packet_size(uint64_t packet_size)
{
    g_packet_size = packet_size;
}
