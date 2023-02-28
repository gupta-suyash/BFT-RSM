#include "parser.h"
#include <fstream>
#include <jsoncpp/json/json.h>
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

    std::string pathToConfig =
        argv[1]; //"/proj/ove-PG0/murray/Scrooge/Code/experiments/experiment_json/scale_clients.json";
    std::ifstream configFile(pathToConfig, std::ifstream::binary);
    Json::Value config;
    configFile >> config;
    // Test print statement
    // SPDLOG_CRITICAL("Here is the entire json object: {}", config);

    const bool useDebugLogs = std::stoull(config[argv[2]][argsIndex][argv[3]]["use_debug_logs_bool"].asString());
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
        // ID the node has within the group it is in, e.g. it could be node 0, 1 etc.
        const auto ownNodeId =
            std::stoull(config[argv[2]][argsIndex][argv[3]]["node_id"].asString()); // std::stoull(argv[2]);
        // ID of the group the node is in
        const auto ownNetworkId = std::stoull(config[argv[2]][argsIndex][argv[3]]["own_network_id"].asString());
        // Size of the network the node is in
        const auto ownNetworkSize = std::stoull(config[argv[2]][argsIndex][argv[3]]["local_num_nodes"].asString());
        // Size of the network the node is not in
        const auto otherNetworkSize = std::stoull(config[argv[2]][argsIndex][argv[3]]["foreign_num_nodes"].asString());
        // Maximum number of nodes allowed to fail in this node's network
        const auto ownNetworkMaxNodesFail =
            std::stoull(config[argv[2]][argsIndex][argv[3]]["local_max_nodes_fail"].asString());
        // Maximum number of nodes allowed to fail in other network that this node is not a member of
        const auto otherNetworkMaxNodesFail =
            std::stoull(config[argv[2]][argsIndex][argv[3]]["foreign_max_nodes_fail"].asString());
        // Number of packets to send
        const auto numPackets = std::stoi(config[argv[2]][argsIndex][argv[3]]["num_packets"].asString());
        // Size of the packets to send
        const auto packetSize = std::stoi(config[argv[2]][argsIndex][argv[3]]["packet_size"].asString());
        // Path to the directory where logfiles should be written to
        const auto logDir = config[argv[2]][argsIndex][argv[3]]["log_path"].asString();
        std::string log_prefix = "log_";
        std::string txt_suffix = ".txt";
        const auto logPath = logDir + log_prefix + std::to_string(ownNodeId) + txt_suffix;
        SPDLOG_INFO("Log Path: {}", logPath);
        //	auto logger = spdlog::basic_logger_mt("basic_logger", logPath);
        //	spdlog::set_default_logger(logger);

        set_packet_size(packetSize);
        set_number_of_packets(numPackets);
        set_rsm_id(ownNetworkId);
        set_other_rsm_id(1 - ownNetworkId);
        return NodeConfiguration{.kOwnNetworkSize = ownNetworkSize,
                                 .kOtherNetworkSize = otherNetworkSize,
                                 .kOwnMaxNumFailedNodes = ownNetworkMaxNodesFail,
                                 .kOtherMaxNumFailedNodes = otherNetworkMaxNodesFail,
                                 .kNodeId = ownNodeId,
                                 .kLogPath = logPath};
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
