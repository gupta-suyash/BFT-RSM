#include "parser.h"

#include <string>

/* Parses commandline options.
 *
 * We assume that the node id's are consecutive numbers, starting from 0, such that
 * all the nodes belonging to one RSM have consecutive ids.
 *
 */
NodeConfiguration parser(int argc, char * argv[])
{
    constexpr auto kNumArgs = 8 + 1;
    if (argc != kNumArgs)
    {
        // we should really use an existing parser like boost::program_options
        SPDLOG_CRITICAL("EXPECTED {} ARGS, RECEIVED {}", kNumArgs - 1, argc - 1);

        SPDLOG_INFO("Run as ./scrooge node_id use_debug_bool local_max_nodes_fail foreign_max_nodes_fail local_num_nodes foreign_num_nodes num_packets packet_size");
        exit(1);
    }

    for (int i = 1; i < argc; i++)
    {
        switch (i)
        {
        case 1:
            set_node_id(stoi(argv[1]));
            break;
        case 2:
            if ("1"s == argv[2])
            {
                spdlog::set_level(spdlog::level::debug);
            }
            else
            {
                spdlog::set_level(spdlog::level::info);
            }
            break;
        case 3:
            // Byzantine nodes in this cluster
            set_max_nodes_fail(true, std::stoi(argv[3]));
            break;
        case 4:
            // Byzantine nodes in the other cluster
            set_max_nodes_fail(false, std::stoi(argv[4]));
            break;
        case 5:
            // Number of nodes in this cluster
            set_num_of_nodes_rsm(true, std::stoi(argv[5]));
            break;
        case 6:
            // Number of nodes in the other cluster
            set_num_of_nodes_rsm(false, std::stoi(argv[6]));
            // As nodes have consecutive id, simple division helps to
            // set the rsm id (rsm to which this node belongs).
            set_rsm_id((get_node_id() / get_nodes_rsm()));

            // The id of each node w.r.t to its rsm (starting from 0).
            set_node_rsm_id(get_node_id() % get_nodes_rsm());

            set_other_rsm_id(get_num_of_rsm() - 1 - get_rsm_id());
            break;
        case 7:
            // Number of packets
            set_number_of_packets(std::stoi(argv[7]));
            break;
        case 8:
            // Packet size
            set_packet_size(std::stoi(argv[8]));
            break;
        default:
            SPDLOG_ERROR("UNEXPECTED COMMAND LINE ARGUMENT position={}, arg='{}'", i, argv[i]);
        }
    }

    SPDLOG_INFO("COUNT: {}", g_node_cnt);

    return NodeConfiguration{
        .kOwnNetworkSize = get_nodes_rsm(),
        .kOtherNetworkSize = get_nodes_other_rsm(),
        .kOwnMaxNumFailedNodes = get_max_nodes_fail(true),
        .kOtherMaxNumFailedNodes = get_max_nodes_fail(false),
        .kNodeId = get_node_id()
    };
}
