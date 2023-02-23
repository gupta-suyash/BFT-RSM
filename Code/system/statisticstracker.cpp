#include "global.h"
#include <map>
#include <vector>
#include <iostream>
#include <fstream>
#include <chrono>

class StatisticsInterpreter {
    std::mutex sendThread;
    std::vector<size_t> latencies;
    std::map<size_t, std::chrono::high_resolution_clock::time_point> packet_num_to_start_time; // TODO fixed size

    public:
    	void startTimer(size_t packet_num) {
	    packet_num_to_start_time.insert(std::pair(packet_num, std::chrono::high_resolution_clock::now()));
	}

    	double recordLatency(size_t packet_num) {
	    auto end = std::chrono::high_resolution_clock::now();
	    if (packet_num_to_start_time.count(packet_num)) {
		    SPDLOG_CRITICAL("Packet number is not found!");
		    return -1;
	    }
	    std::chrono::duration<double> diff = end - packet_num_to_start_time.at(packet_num);
	    latencies.push_back(diff.count());
	    removeTimeStamp(packet_num);
	    return diff.count();
	}

	void removeTimeStamp(size_t packet_num) {
	    packet_num_to_start_time.erase(packet_num);
	}

    	void printOutAllResults() {
	    std::ofstream myfile;
	    myfile.open ("results.txt");
	    for (size_t latency : latencies) {
		    myfile << latency;
	    }
	    myfile.close();
    }
};
