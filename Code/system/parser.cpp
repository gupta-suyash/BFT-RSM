#include "parser.h"
#include <fstream>
#include <jsoncpp/json/json.h>
#include <string>
#include <iostream>

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
    constexpr auto kNumArgs = 7 + 1;
    if (argc != kNumArgs)
    {
        // we should really use an existing parser like boost::program_options
        SPDLOG_CRITICAL("EXPECTED {} ARGS, RECEIVED {}", kNumArgs - 1, argc - 1);
        usage();
    }

    const auto kPathToConfig = argv[1];
    const auto kExperimentName = argv[2];
    const auto kConfigId = argv[3];
    const auto kPersonalId = argv[4];
    const Json::ArrayIndex kRoundNb = stoull(argv[5]);
    set_port_numbers(stoull(argv[6]), stoull(argv[7]));
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
    std::string cluster = "cluster_"s + kConfigId;
    const auto kOwnParams = kScroogeParams[cluster];
    const auto kGeneralParams = kScroogeParams["general"];

    if (config.isNull() || kScroogeParams.isNull() || kOwnParams.isNull() || kGeneralParams.isNull())
    {
        SPDLOG_CRITICAL("Invalid config found at path {}, configIsNull={}, scroogeParamsIsNull={}, ownParamsIsNull={}, generalParamsIsNull = {}",
                        kPathToConfig, config.isNull(), kScroogeParams.isNull(), kOwnParams.isNull(), kGeneralParams.isNull());
        usage();
    }

    const bool useDebugLogs = kGeneralParams["use_debug_logs_bool"].asBool();
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
	const auto workingDir = config["experiment_independent_vars"]["network_dir"].asString();

        const auto ownNodeId = stoull(kPersonalId);

        const auto ownNetworkId = stoull(kConfigId);
        
	const auto ownNetworkSize = kOwnParams["local_num_nodes"][kRoundNb].asUInt64();
        
	const auto otherNetworkSize = kOwnParams["foreign_num_nodes"][kRoundNb].asUInt64();

        const auto ownNetworkMaxNodesFail = kOwnParams["local_max_nodes_fail"][kRoundNb].asUInt64();

        const auto otherNetworkMaxNodesFail = kOwnParams["foreign_max_nodes_fail"][kRoundNb].asUInt64();
        
	const auto numPackets = kOwnParams["num_packets"][kRoundNb].asUInt64();

        const auto packetSize = kOwnParams["packet_size"][kRoundNb].asUInt64();

        const auto logDir = kOwnParams["log_path"].asString();
        
	const auto logPath = logDir + "log_"s + std::to_string(ownNodeId) + ".txt"s;
        SPDLOG_INFO("Working directory: {}", workingDir);
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
                                 .kLogPath = logPath,
				 .kWorkingDir = workingDir};
    }
    catch (const std::runtime_error& re)
    {
	std::cerr << "Runtime error: " << re.what() << std::endl;
    }
    catch (const std::exception& ex)
    {
	    std::cerr << "Error occurred: " << ex.what() << std::endl;
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
