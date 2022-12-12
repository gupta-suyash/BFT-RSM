#pragma once

#include "global.h"

#include <filesystem>
#include <vector>

NodeConfiguration parser(int argc, char *argv[]);

std::vector<std::string> parseNetworkUrls(const std::filesystem::path &networkConfigPath);
