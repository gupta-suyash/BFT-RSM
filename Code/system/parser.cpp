#include "parser.h"

#include <fstream>
#include <string>

void usage()
{
    SPDLOG_INFO("Run as ./scrooge use_debug_logs_bool node_id local_num_nodes foreign_num_nodes local_max_nodes_fail "
                "foreign_max_nodes_fail  own_network_id num_packets packet_size");
    exit(1);
}

/* Parses commandline options.
 *
 * We assume that the node id's are consecutive numbers, starting from 0, such that
 * all the nodes belonging to one RSM have consecutive ids.
 *
 */
NodeConfiguration parser(int argc, char *argv[])
{
    constexpr auto kNumArgs = 3 + 1;
    if (argc != kNumArgs)
    {
        // we should really use an existing parser like boost::program_options
        SPDLOG_CRITICAL("EXPECTED {} ARGS, RECEIVED {}", kNumArgs - 1, argc - 1);
        usage();
    }

    const bool useDebugLogs = "1"s == argv[1];
    if (useDebugLogs)
    {
        spdlog::set_level(spdlog::level::debug);
    }
    else
    {
        spdlog::set_level(spdlog::level::info);
    }

    try
    {
        const auto ownNodeId = std::stoull(argv[2]);
        const auto ownNetworkSize = 4;
        const auto otherNetworkSize = 4;
        const auto ownNetworkMaxNodesFail = 1;
        const auto otherNetworkMaxNodesFail = 1;
        const auto ownNetworkId = std::stoi(argv[3]);
        const auto numPackets = 100;
        const auto packetSize = 1;
        set_packet_size(packetSize);
        set_number_of_packets(numPackets);
        set_rsm_id(ownNetworkId);
        set_other_rsm_id(1 - ownNetworkId);

        return NodeConfiguration{.kOwnNetworkSize = ownNetworkSize,
                                 .kOtherNetworkSize = otherNetworkSize,
                                 .kOwnMaxNumFailedNodes = ownNetworkMaxNodesFail,
                                 .kOtherMaxNumFailedNodes = otherNetworkMaxNodesFail,
                                 .kNodeId = ownNodeId};
    }
    catch (...)
    {
        SPDLOG_CRITICAL("Cannot parse integer command line arguments");
        usage();
        return NodeConfiguration{};
    }
}

std::vector<std::string> parseNetworkUrls(const std::filesystem::path &networkConfigPath)
{
    auto input = std::ifstream{networkConfigPath};
    if (!input)
    {
        SPDLOG_CRITICAL("Error opening file {} for reading", networkConfigPath.c_str());
        exit(1);
    }

    std::string ipAddress;
    std::vector<std::string> ipAddresses{""};

    while (std::getline(input, ipAddresses.back()))
    {
        ipAddresses.emplace_back("");
    }
    ipAddresses.pop_back();
    return ipAddresses;
}
