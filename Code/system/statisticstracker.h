#include "global.h"

#include <chrono>
#include <utility>

#include <boost/circular_buffer.hpp>

extern uint64_t ack_count;
extern long double tot_lat;
extern boost::circular_buffer<std::pair<uint64_t, std::chrono::steady_clock::time_point>> latency_map;

void startTimer(uint64_t packet_num, std::chrono::steady_clock::time_point now);
void recordLatency(uint64_t curQuack, std::chrono::steady_clock::time_point now);
double averageLat();
void allToall(std::chrono::steady_clock::time_point start_time);
