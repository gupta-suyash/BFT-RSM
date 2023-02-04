#include <map>
#include <vector>
#include <iostream>
#include <fstream>
#include <chrono>

class StatisticsInterpreter {
    std::vector<size_t> latencies;
    std::map<size_t, std::chrono::high_resolution_clock::time_point> packet_num_to_start_time;

    public:
    	void startTimer(size_t packet_num) {
	    packet_num_to_start_time.insert(std::pair<size_t, std::chrono::high_resolution_clock::time_point>(packet_num, std::chrono::high_resolution_clock::now()));
    }

    	void endTimer(size_t packet_num) {
	    auto end = std::chrono::high_resolution_clock::now();
	    std::chrono::duration<double> diff = packet_num_to_start_time.at(packet_num) - end;
	    latencies.push_back(diff.count());
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
