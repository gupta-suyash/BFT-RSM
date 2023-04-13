#include <iostream>
#include <fstream>
#include <chrono>
#include <vector>
#include <unordered_map>

#include "global.h"

extern uint64_t ack_count;
extern double tot_lat;
extern std::unordered_map<uint64_t, std::chrono::high_resolution_clock::time_point> latency_map; // TODO fixed size

void startTimer(uint64_t packet_num);
void recordLatency(uint64_t lastQuack, uint64_t curQuack);
void removeTimeStamp(uint64_t packet_num);
double averageLat();
void allToall(std::chrono::high_resolution_clock::time_point start_time);
