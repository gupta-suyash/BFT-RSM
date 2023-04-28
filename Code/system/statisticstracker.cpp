#include "statisticstracker.h"

// Variables
uint64_t ack_count = 0;
double tot_lat = 0;
std::unordered_map<uint64_t, std::chrono::high_resolution_clock::time_point> latency_map;

// Functions
void startTimer(uint64_t seq_num)
{
    latency_map[seq_num] = std::chrono::high_resolution_clock::now();
}

void recordLatency(uint64_t lastQuack, uint64_t curQuack)
{
    while (lastQuack < curQuack)
    {
        auto end = std::chrono::high_resolution_clock::now();
        if (!latency_map.count(lastQuack))
        {
            SPDLOG_INFO("Message not found: L:{} :: Q:{}", lastQuack, curQuack);
            break;
        }
        std::chrono::duration<double> diff = end - latency_map.at(lastQuack);
        tot_lat += diff.count();
        ack_count++;
        removeTimeStamp(lastQuack);
        lastQuack++;
    }
}

void removeTimeStamp(uint64_t packet_num)
{
    latency_map.erase(packet_num);
}

double averageLat()
{
    addMetric("Ack Count", ack_count);
    return tot_lat / (ack_count * 1.0);
}

void allToall(std::chrono::high_resolution_clock::time_point start_time)
{
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff = end - start_time;
    tot_lat += diff.count();
    ack_count++;
}