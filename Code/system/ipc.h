#pragma once

#include "global.h"

#include <atomic>
#include <boost/lockfree/spsc_queue.hpp>
#include <string>
#include <vector>

bool createPipe(const std::string &path);

void startPipeReader(const std::string &path, boost::lockfree::spsc_queue<std::vector<uint8_t>> *const messageReads,
                     const std::atomic_bool &exit);

void startPipeWriter(const std::string &path, boost::lockfree::spsc_queue<std::vector<uint8_t>> *const messageWrites,
                     const std::atomic_bool &exit);
