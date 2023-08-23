#pragma once

#include "config.h"

//#define NDEBUG
#include <chrono>
#include <cstdlib>
#include <string>
#include <type_traits>
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_CRITICAL
#include <spdlog/spdlog.h>

using namespace std::string_literals;
using namespace std::chrono_literals;

constexpr uint64_t kListSize = KLIST_SIZE;
static_assert(kListSize % 64 == 0, "kListSize must be a multiple of 64");

struct NodeConfiguration
{
    const uint64_t kOwnNetworkSize;
    const uint64_t kOtherNetworkSize;
    const std::vector<uint64_t> kOwnNetworkStakes;
    const std::vector<uint64_t> kOtherNetworkStakes;
    const uint64_t kOwnMaxNumFailedStake;
    const uint64_t kOtherMaxNumFailedStake;
    const uint64_t kNodeId;
    const std::string kLogPath;
    const std::string kWorkingDir;
};

void set_test_start(std::chrono::steady_clock::time_point startTime);
void start_recording();
void end_test();
std::chrono::duration<double> get_test_duration();
std::chrono::duration<double> get_test_warmup_duration();
bool is_test_over();
bool is_test_recording();

void set_priv_key();
std::string get_priv_key();

void set_own_rsm_key(uint64_t nid, std::string pkey);
std::string get_own_rsm_key(uint64_t nid);

void set_other_rsm_key(uint64_t nid, std::string pkey);
std::string get_other_rsm_key(uint64_t nid);

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

void bindThreadAboveCpu(const int cpu);

void addMetric(std::string key, std::string value);

template <typename NumericType, std::enable_if_t<std::is_arithmetic_v<NumericType>, bool> = true>
void addMetric(std::string key, NumericType value)
{
    addMetric(key, std::to_string(value));
}

void printMetrics(std::string filename);
