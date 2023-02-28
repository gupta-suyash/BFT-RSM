#include "parser.h"
#include <fstream>
#include <jsoncpp/json/json.h>
#include <string>

void usage()
{
    SPDLOG_INFO("Run as ./scrooge path_to_config exp_name node_id");
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

    const auto kPathToConfig = argv[1];
    const auto kExperimentName = argv[2];
    const auto kConfigId = argv[3];

    std::ifstream configFile(kPathToConfig, std::ifstream::binary);
    Json::Value config;
    try
    {
        configFile >> config;
    }
    catch (...)
    {
        SPDLOG_CRITICAL("Could not find config at path {}", kPathToConfig);
    }

    const auto kScroogeParams = config[kExperimentName]["scrooge_args"];
    const auto kOwnParams = kScroogeParams[kConfigId];

    if (config.isNull() || kScroogeParams.isNull() || kOwnParams.isNull())
    {
        SPDLOG_CRITICAL("Invalid config found at path {}, configIsNull={}, scroogeParamsIsNull={}, ownParamsIsNull={}",
                        kPathToConfig, config.isNull(), kScroogeParams.isNull(), kOwnParams.isNull());
        usage();
    }

    const bool useDebugLogs = kOwnParams["use_debug_logs_bool"].asBool();
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

        const auto ownNodeId = kOwnParams["node_id"].asUInt64();

        const auto ownNetworkId = kOwnParams["own_network_id"].asUInt64();

        const auto ownNetworkSize = kOwnParams["local_num_nodes"].asUInt64();

        const auto otherNetworkSize = kOwnParams["foreign_num_nodes"].asUInt64();

        const auto ownNetworkMaxNodesFail = kOwnParams["local_max_nodes_fail"].asUInt64();

        const auto otherNetworkMaxNodesFail = kOwnParams["foreign_max_nodes_fail"].asUInt64();

        const auto numPackets = kOwnParams["num_packets"].asUInt64();

        const auto packetSize = kOwnParams["packet_size"].asUInt64();

        const auto logDir = kOwnParams["log_path"].asString();
        const auto logPath = logDir + "log_"s + std::to_string(ownNodeId) + ".txt"s;
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
        SPDLOG_CRITICAL("Cannot parse config ints properly");
        usage();
        return NodeConfiguration{}; // unreachable
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
