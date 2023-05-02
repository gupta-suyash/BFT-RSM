#include "statisticstracker.h"

// Variables
uint64_t ack_count = 0;
long double tot_lat = 0;
boost::circular_buffer<std::pair<uint64_t, std::chrono::steady_clock::time_point>> latency_map(3 * (1<<20));

// Functions
void startTimer(uint64_t seq_num, std::chrono::steady_clock::time_point now)
{
    latency_map.push_back(std::make_pair(seq_num, now));
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
    auto end = std::chrono::steady_clock::now();
    std::chrono::duration<long double> diff = end - start_time;
    tot_lat += diff.count();
    ack_count++;
}
