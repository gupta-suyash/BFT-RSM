#include "statisticstracker.h"

// Variables
uint64_t ack_count = 0;
double tot_lat = 0;
std::unordered_map<uint64_t, std::chrono::high_resolution_clock::time_point> client_latency_map;

// Functions
void startClientTimer(uint64_t seq_num)
{
    client_latency_map[seq_num] = std::chrono::high_resolution_clock::now();
}

void recordClientLatency(uint64_t seq_num)
{
    auto end = std::chrono::high_resolution_clock::now();
    if (!client_latency_map.count(seq_num))
    {
        SPDLOG_INFO("Message not found for seq_num {}", seq_num);
        return;
    }
    std::chrono::duration<double> diff = end - client_latency_map.at(seq_num);
    tot_lat += diff.count();
    ack_count++;
    removeTimeStamp(seq_num);
}

void removeClientTimeStamp(uint64_t packet_num)
{
    client_latency_map.erase(packet_num);
}

double averageClientLat()
{
    return tot_lat / (ack_count * 1.0);
}

uint64_t ackCount()
{
    return ack_count;
}
