#include "global.h"
#include "crypto.h"
#include <atomic>
#include <bitset>
#include <fstream>
#include <map>
#include <mutex>
#include <pthread.h>
#include <sched.h>
#include <sys/sysinfo.h>
#include <unordered_map>

// List of global variables and configuration parameters.
static uint64_t g_rsm_id{};       // RSM Id for this node.
static uint64_t g_other_rsm_id{}; // RSM Id of other RSM.
static uint64_t g_number_of_packets{};
static uint64_t g_packet_size{};

static std::mutex metricsMutex{};
static std::map<std::string, std::string> metrics{};

std::string privKey;
std::unordered_map<uint64_t, std::string> keyOwnCluster;
std::unordered_map<uint64_t, std::string> keyOtherCluster;

static std::chrono::steady_clock::time_point g_start_time{};
static constexpr auto kWarmupDuration = 40s;
static constexpr auto kTestDuration = 120s;

static std::atomic_bool isTestOver{};
static std::atomic_bool isTestRecording{};

void set_priv_key()
{
    // privKey = CmacGenerateHexKey();
    privKey = "00000000000000000000000000000000";
    // std::cout << "Key: " << privKey << std::endl;
}

std::string get_priv_key()
{
    return privKey;
}

void set_own_rsm_key(uint64_t nid, std::string pkey)
{
    keyOwnCluster[nid] = pkey;
}

std::string get_own_rsm_key(uint64_t nid)
{
    return keyOwnCluster[nid];
}

void set_other_rsm_key(uint64_t nid, std::string pkey)
{
    keyOtherCluster[nid] = pkey;
}

std::string get_other_rsm_key(uint64_t nid)
{
    return keyOtherCluster[nid];
}

void set_test_start(std::chrono::steady_clock::time_point startTime)
{
    g_start_time = startTime;
}

std::chrono::duration<double> get_test_duration()
{
    return kTestDuration - kWarmupDuration;
}

std::chrono::duration<double> get_test_warmup_duration()
{
    return kWarmupDuration;
}

void start_recording()
{
    isTestRecording = true;
}

void end_test()
{
    isTestOver = true;
}

bool is_test_over()
{
    return isTestOver.load(std::memory_order_relaxed);
}

bool is_test_recording()
{
    return isTestRecording.load(std::memory_order_relaxed);
}

/* Get the id of the RSM this node belongs.
 *
 * @return g_rsm_id.
 */
uint64_t get_rsm_id()
{
    return g_rsm_id;
}

/* Set the id of the RSM this node belongs.
 *
 * @param rsm_id is the RSM id.
 */
void set_rsm_id(uint64_t rsm_id)
{
    g_rsm_id = rsm_id;
}

/* Get the id of the other RSM.
 *
 * @return g_other_rsm_id.
 */
uint64_t get_other_rsm_id()
{
    return g_other_rsm_id;
}

/* Set the id of the other RSM.
 *
 * @param rsm_id is the RSM id.
 */
void set_other_rsm_id(uint64_t rsm_id)
{
    g_other_rsm_id = rsm_id;
}

uint64_t get_number_of_packets()
{
    return g_number_of_packets;
}

void set_number_of_packets(uint64_t packet_number)
{
    g_number_of_packets = packet_number;
}

uint64_t get_packet_size()
{
    return g_packet_size;
}

void set_packet_size(uint64_t packet_size)
{
    g_packet_size = packet_size;
}

void bindThreadToCpu(const int cpu)
{
    static std::mutex mutex;
    static std::bitset<128> set{};
    const auto numCores = get_nprocs();
    {
        std::scoped_lock lock{mutex};
        if (set.test(cpu) || cpu >= numCores)
        {
            SPDLOG_CRITICAL("Cannot allocate a unique core for this thread, num_cores={}, requested={}", numCores, cpu);
            std::abort();
        }
        set.set(cpu);
    }

    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu, &cpuset);

    int rc = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
    if (rc != 0)
    {
        SPDLOG_CRITICAL("Cannot bind this thread to desired core error={}, num_cores={}, requested={}", rc, numCores,
                        cpu);
        std::abort();
    }
}

void bindThreadAboveCpu(const int cpu)
{
    const auto numCores = get_nprocs();

    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    for (int i = cpu + 1; i < numCores; i++)
    {
        CPU_SET(i, &cpuset);
    }

    int rc = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
    if (rc != 0)
    {
        SPDLOG_CRITICAL("Cannot bind this thread above desired core error={}, num_cores={}, requested={}", rc, numCores,
                        cpu);
        std::abort();
    }
}

void addMetric(std::string key, std::string value)
{
    std::scoped_lock lock{metricsMutex};
    metrics[key] = value;
}

void printMetrics(std::string filename)
{
    std::scoped_lock lock{metricsMutex};
    remove(filename.c_str());
    std::ofstream file{filename, std::ios_base::binary};
    for (const auto &metric : metrics)
    {
        const auto &[metricKey, metricValue] = metric;
        file << metricKey << ": " << metricValue << '\n';
    }
}
