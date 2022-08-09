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

#include "global.h"

using std::filesystem::current_path;

#define NODE0 "node0"
#define NODE1 "node1"


class Pipeline {
	string path_cwd_; // Path to current working directory.
	vector<string> ip_addr; // IP addresses of own RSM.
public:			    
	string GetPath();
	void ReadIfconfig(string if_path);
	string getIP(UInt16 id); 

	int NodeReceive(const char *url);
	int NodeSend(const char *url);
	 
};	


#endif
