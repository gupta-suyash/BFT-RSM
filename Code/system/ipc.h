#pragma once

#include "global.h"

#include <memory>
#include <string>
#include <vector>

#include <boost/fiber/buffered_channel.hpp>

namespace ipc
{
using DataChannel = boost::fibers::buffered_channel<std::vector<uint8_t>>;
}; // namespace ipc

bool createPipe(const std::string &path);

void startPipeReader(std::string path, std::shared_ptr<ipc::DataChannel> messageReads);

void startPipeWriter(std::string path, std::shared_ptr<ipc::DataChannel> messageWrites);
