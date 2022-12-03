#pragma once

#include "global.h"

#include <boost/lockfree/queue.hpp>
#include <string>
#include <vector>

bool createPipe(const std::string &path);

void startPipeReader(const std::string &path, boost::lockfree::queue<std::vector<uint8_t>> *const messageReads,
                     std::atomic_bool exit, const std::atomic_bool &);

void startPipeWriter(const std::string &path, boost::lockfree::queue<std::vector<uint8_t>> *const messageWrites,
                     std::atomic_bool exit, const std::atomic_bool &);
