#include "statisticstracker.h"

// Variables
uint64_t ack_count = 0;
long double tot_lat = 0;
boost::circular_buffer<std::pair<uint64_t, std::chrono::steady_clock::time_point>> latency_map(3 * (1<<20));
std::optional<std::chrono::steady_clock::time_point> firstMeasurement{};

// Functions
void startTimer(uint64_t seq_num, std::chrono::steady_clock::time_point now)
{
    if (is_test_recording())
    {
        if (not firstMeasurement)
        {
            firstMeasurement = now;
        }
        latency_map.push_back(std::make_pair(seq_num, now));
    }
}

void recordLatency(uint64_t curQuack, std::chrono::steady_clock::time_point now)
{
    while (not latency_map.empty())
    {
        const auto& [seq_num, start_time] = latency_map.front();
        if (seq_num > curQuack)
        {
            return;
        }
        tot_lat += std::chrono::duration<long double>(now - start_time).count();
        ack_count++;
        latency_map.pop_front();
    }
}

double averageLat()
{
    addMetric("Ack Count", ack_count);
    return tot_lat / (ack_count * 1.0);
}

void allToall(std::chrono::steady_clock::time_point start_time)
{
    if (not firstMeasurement.has_value())
    {
        SPDLOG_CRITICAL("Never recorded a latency");
        std::abort();
    }
    auto end = std::chrono::steady_clock::now();
    std::chrono::duration<long double> diff = end - firstMeasurement.value();
    tot_lat += diff.count();
    ack_count++;
}
