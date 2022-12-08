#include "global.h"
#include <vector>
#include <string>

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
    std::vector<uint64_t> num_args = {};
    
    set_number_of_packets(std::stoi(argv[3]));

    for (int i = 1; i < argc; i++)
    {
        //if (i == 1 && argv[i][0] == 'n' && argv[i][1] == 'i' && argv[i][2] == 'd')
        if (i == 1)
	{
            //set_node_id(stoi(&argv[i][3]));
	    set_node_id(stoi(argv[i]));

            // As nodes have consecutive id, simple division helps to
            // set the rsm id (rsm to which this node belongs).
            set_rsm_id((get_node_id() / get_nodes_rsm()));

            // The id of each node w.r.t to its rsm (starting from 0).
            set_node_rsm_id(get_node_id() % get_nodes_rsm());

            set_other_rsm_id(get_num_of_rsm() - 1 - get_rsm_id());
	    continue;
        }

	// 1 == DEBUG, anything else == INFO
	if (i == 2) {
	   if (std::stoi(argv[i]) == 1) {
	     spdlog::set_level(spdlog::level::debug);
	   } else {
             spdlog::set_level(spdlog::level::info);
	   }
	   continue;
	}
	
	if (i == 3) { // Byzantine nodes in this cluster
	   set_max_nodes_fail(true, std::stoi(argv[3]));
	   continue;
	}

	if (i == 4) { // Byzantine nodes in the other cluster
	   set_max_nodes_fail(false, std::stoi(argv[4]));
	   continue;
	}

	if (i == 5) { // Number of nodes in this cluster
	   set_num_of_nodes_rsm(true, std::stoi(argv[5]));
	   continue;
	}

	if (i == 6) { // Number of nodes in the other cluster
	   set_num_of_nodes_rsm(false, std::stoi(argv[6]));
	   continue;
	}

	if (i == 7) { // Number of packets
	   set_number_of_packets(std::stoi(argv[7]));
	   continue;
	}

	if (i == 8) { // Packet size
	   set_packet_size(std::stoi(argv[8]));
	   continue;
	}
    }

    // Set log level
    // TODO: set based on user input
}
