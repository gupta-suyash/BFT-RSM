#pragma once

//#define NDEBUG
#include <chrono>
// Enable all spdlog logging macros for development
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_CRITICAL
#include <spdlog/spdlog.h>

using namespace std;
using namespace std::chrono_literals;

struct NodeConfiguration
{
    const uint64_t kOwnNetworkSize;
    const uint64_t kOtherNetworkSize;
    const uint64_t kOwnMaxNumFailedNodes;
    const uint64_t kOtherMaxNumFailedNodes;
    const uint64_t kNodeId;
    const std::string kLogPath;
    const std::string kWorkingDir;
};

void set_test_start(std::chrono::steady_clock::time_point startTime);
std::chrono::duration<double> get_test_duration();
bool is_test_over();
bool is_test_recording();

uint64_t get_rsm_id();
void set_rsm_id(uint64_t rsm_id);

uint64_t get_other_rsm_id();
void set_other_rsm_id(uint64_t rsm_id);

uint64_t get_max_nodes_fail(bool thisNodeRsm);
void set_max_nodes_fail(bool thisNodeRsm, uint64_t max_nodes_fail);

uint64_t get_number_of_packets();
void set_number_of_packets(uint64_t packet_number);

uint64_t get_packet_size();
void set_packet_size(uint64_t packet_size);

// Will set the current thread to be bound to a unique CPU
// Will terminate program with error msg if there are not enough CPUs or if operation fails
void bindThreadToCpu(int cpu);
