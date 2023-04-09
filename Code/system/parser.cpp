#include "parser.h"
#include <fstream>
#include <iostream>
#include <jsoncpp/json/json.h>
#include <string>

using namespace std;
using namespace std::chrono_literals;

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
parser::CommandLineArguments parseCommandLineArguments(int argc, char *argv[])
{
    constexpr auto kNumArgs = 5 + 1;
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
    const Json::ArrayIndex kRoundNb = std::stoull(argv[5]);

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
        SPDLOG_CRITICAL("Invalid config found at path {}, configIsNull={}, scroogeParamsIsNull={}, ownParamsIsNull={}, "
                        "generalParamsIsNull = {}",
                        kPathToConfig, config.isNull(), kScroogeParams.isNull(), kOwnParams.isNull(),
                        kGeneralParams.isNull());
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

        const auto ownNodeId = std::stoull(kPersonalId);

        const auto ownNetworkId = stoull(kConfigId);

        const auto ownNetworkSize = kOwnParams["local_num_nodes"][kRoundNb].asUInt64();

        const auto otherNetworkSize = kOwnParams["foreign_num_nodes"][kRoundNb].asUInt64();

        const auto ownNetworkMaxNodesFail = kOwnParams["local_max_nodes_fail"][kRoundNb].asUInt64();

        const auto otherNetworkMaxNodesFail = kOwnParams["foreign_max_nodes_fail"][kRoundNb].asUInt64();

        const auto numPackets = kOwnParams["num_packets"][kRoundNb].asUInt64();

        const auto packetSize = kOwnParams["packet_size"][kRoundNb].asUInt64();

        const auto logDir = kOwnParams["log_path"].asString();

        const auto logPath =
            logDir + "/tmp/log_" + std::to_string(ownNetworkId) + "_" + std::to_string(ownNodeId) + ".yaml";
        SPDLOG_INFO("Network directory: {}", workingDir);

        set_packet_size(packetSize);
        set_number_of_packets(numPackets);
        set_rsm_id(ownNetworkId);
        set_other_rsm_id(1 - ownNetworkId);
        return parser::CommandLineArguments{.kOwnNetworkSize = ownNetworkSize,
                                            .kOtherNetworkSize = otherNetworkSize,
                                            .kOwnMaxNumFailedStake = ownNetworkMaxNodesFail,
                                            .kOtherMaxNumFailedStake = otherNetworkMaxNodesFail,
                                            .kNodeId = ownNodeId,
                                            .kLogPath = logPath,
                                            .kWorkingDir = workingDir};
    }
    catch (const std::runtime_error &re)
    {
        std::cerr << "Runtime error: " << re.what() << std::endl;
    }
    catch (const std::exception &ex)
    {
        std::cerr << "Error occurred: " << ex.what() << std::endl;
    }
    catch (...)
    {
        SPDLOG_CRITICAL("Cannot parse config ints properly");
    }
    usage();
    return parser::CommandLineArguments{}; // unreachable
}

parser::ConfigurationParameters parseNetworkUrlsAndStake(const std::filesystem::path &networkConfigPath)
{
    auto input = std::ifstream{networkConfigPath};
    if (!input)
    {
        SPDLOG_CRITICAL("Error opening file {} for reading", networkConfigPath.c_str());
        exit(1);
    }

    constexpr auto urlDelimiter = ' ';
    std::vector<std::string> networkUrls{};
    std::vector<uint64_t> stakes{};

    while (true)
    {
        std::string url;
        bool noInput = std::getline(input, url, urlDelimiter).fail();
        if (noInput)
        {
            break;
        }
        networkUrls.push_back(url);

        std::string stakeString;
        bool missingStake = std::getline(input, stakeString).fail();
        if (missingStake)
        {
            SPDLOG_CRITICAL("NODE WITH IP '{}' HAD NO STAKE VALUE, exiting...", url);
            std::abort();
        }

        try
        {
            uint64_t stake = std::stoull(stakeString);
            if (stake == 0)
            {
                SPDLOG_CRITICAL("NODE WITH IP '{}' HAD ZERO STAKE, not supported, exiting...", url);
            }
            stakes.push_back(stake);
        }
        catch (...)
        {
            SPDLOG_CRITICAL("COULD NOT PARSE STAKE STRING '{}', exiting...", stakeString);
            std::abort();
        }
    }

    return parser::ConfigurationParameters{.kNetworkUrls = networkUrls, .kNetworkStakes = stakes};
}

NodeConfiguration createNodeConfiguration(parser::CommandLineArguments args,
                                          parser::ConfigurationParameters ownNetworkParams,
                                          parser::ConfigurationParameters otherNetworkParams)
{
    const auto config = NodeConfiguration{
        .kOwnNetworkSize = args.kOwnNetworkSize,
        .kOtherNetworkSize = args.kOtherNetworkSize,
        .kOwnNetworkStakes = ownNetworkParams.kNetworkStakes,
        .kOtherNetworkStakes = otherNetworkParams.kNetworkStakes,
        .kOwnMaxNumFailedStake = args.kOwnMaxNumFailedStake,
        .kOtherMaxNumFailedStake = args.kOtherMaxNumFailedStake,
        .kNodeId = args.kNodeId,
        .kLogPath = args.kLogPath,
        .kWorkingDir = args.kWorkingDir,
    };

    const bool isInvalid = config.kOwnNetworkStakes.size() != config.kOwnNetworkSize ||
                           config.kOtherNetworkStakes.size() != config.kOtherNetworkSize ||
                           ownNetworkParams.kNetworkUrls.size() != config.kOwnNetworkSize ||
                           otherNetworkParams.kNetworkUrls.size() != config.kOtherNetworkSize;
    if (isInvalid)
    {
        SPDLOG_CRITICAL("Configuration File error, configuration file size ({} or {})!= CLI argument size ({} or {})",
                        config.kOwnNetworkStakes.size(), config.kOtherNetworkStakes.size(), config.kOwnNetworkSize,
                        config.kOtherNetworkSize);
        std::abort();
    }
    return config;
}
