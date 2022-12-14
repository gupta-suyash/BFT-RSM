#pragma once

#include "global.h"

#include <filesystem>
#include <vector>

NodeConfiguration parser(int argc,  char *argv[]);

std::vector<std::string> parseNetworkIps(const std::filesystem::path& networkConfigPath);
