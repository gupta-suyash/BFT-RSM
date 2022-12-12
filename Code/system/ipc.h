#pragma once

#include "global.h"

#include <atomic>
#include <memory>
#include <string>
#include <vector>

#include <boost/lockfree/spsc_queue.hpp>
namespace ipc
{
using DataChannel = boost::lockfree::spsc_queue<std::vector<uint8_t>>;
}; // namespace ipc

bool createPipe(const std::string &path);

void startPipeReader(std::string path, std::shared_ptr<ipc::DataChannel> messageReads,
                     std::shared_ptr<std::atomic_bool> exit);

void startPipeWriter(std::string path, std::shared_ptr<ipc::DataChannel> messageWrites,
                     std::shared_ptr<std::atomic_bool> exit);
