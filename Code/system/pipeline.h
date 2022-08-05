#ifndef _PIPELINE_
#define _PIPELINE_

#include <iostream>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "nng/nng.h"
#include <nng/protocol/pipeline0/pull.h>
#include <nng/protocol/pipeline0/push.h>

using namespace std;
using std::filesystem::current_path;

#define NODE0 "node0"
#define NODE1 "node1"

int node0(const char *url_snd, const char *url_rcv);
int node1(const char *url_snd, const char *url_rcv);


class Pipeline {
	string path_cwd_; // Path to current working directory.
	vector<string> ip_addr; // IP addresses of own RSM.
public:			    
	string GetPath();
	void ReadIfconfig(string if_path);

	
	 
};	


#endif
