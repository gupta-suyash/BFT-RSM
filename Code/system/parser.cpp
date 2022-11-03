#include "global.h"
#include <spdlog/spdlog.h>

/* Parses commandline options.
 * At present only one option is passed, node id.
 * Format: nid<number> :: Example: nid0
 *
 * We assume that the node id's are consecutive numbers, starting from 0, such that
 * all the nodes belonging to one RSM have consecutive ids.
 *
 */
void parser(int argc, char *argv[])
{
    for (int i = 1; i < argc; i++)
    {
        if (argv[i][0] == 'n' && argv[i][1] == 'i' && argv[i][2] == 'd')
        {
            set_node_id(atoi(&argv[i][3]));

            // As nodes have consecutive id, simple division helps to
            // set the rsm id (rsm to which this node belongs).
            set_rsm_id((get_node_id() / get_nodes_rsm()));

            // The id of each node w.r.t to its rsm (starting from 0).
            set_node_rsm_id(get_node_id() % get_nodes_rsm());

            set_other_rsm_id(get_num_of_rsm() - 1 - get_rsm_id());

            break;
        }
    }

    // Set log level
    // TODO: set based on user input
    spdlog::set_level(spdlog::level::debug);
    spdlog::debug("test");
}
