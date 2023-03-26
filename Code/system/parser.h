#pragma once

#include "global.h"

#include <filesystem>
#include <utility>
#include <vector>

namespace parser
{
struct CommandLineArguments
{
    uint64_t kOwnNetworkSize;
    uint64_t kOtherNetworkSize;
    uint64_t kOwnMaxNumFailedStake;
    uint64_t kOtherMaxNumFailedStake;
    uint64_t kNodeId;
    std::string kLogPath;
    std::string kWorkingDir;
};

struct ConfigurationParameters
{
    std::vector<std::string> kNetworkUrls;
    std::vector<uint64_t> kNetworkStakes;
};
} // namespace parser

parser::CommandLineArguments parseCommandLineArguments(int argc, char *argv[]);

parser::ConfigurationParameters parseNetworkUrlsAndStake(const std::filesystem::path &networkConfigPath);

NodeConfiguration createNodeConfiguration(parser::CommandLineArguments args,
                                          parser::ConfigurationParameters ownNetworkParams,
                                          parser::ConfigurationParameters otherNetworkParams);
