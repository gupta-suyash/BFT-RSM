#include <chrono>
#include <fstream>
#include <iostream>
#include <unordered_map>
#include <vector>

#include "global.h"

extern uint64_t ack_count;
extern double tot_lat;
extern std::unordered_map<uint64_t, std::chrono::high_resolution_clock::time_point> client_latency_map; // TODO fixed size

void startClientTimer(uint64_t packet_num);
void recordClientLatency(uint64_t seq_num);
void removeClientTimeStamp(uint64_t packet_num);
double averageClientLat();
uint64_t ackCount();