#ifndef _PIPELINE_
#define _PIPELINE_

#include <iostream>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <thread>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>

#include "nng/nng.h"
#include <nng/protocol/pipeline0/pull.h>
#include <nng/protocol/pipeline0/push.h>

#include "global.h"

using std::filesystem::current_path;

#define NODE0 "node0"
#define NODE1 "node1"


class Pipeline {
	vector<string> ip_addr; // IP addresses of own RSM.
	vector <string> tcp_url;
	vector <thread> athreads_; // Input (Receive) threads.
public:			    
	Pipeline();
	string GetPath();
	void ReadIfconfig(string if_path);
	string getIP(UInt16 id); 

	string GetRecvUrl(UInt16 cnt);
	string GetSendUrl(UInt16 cnt);
	void SetIThreads();

	int NodeReceive();
	int NodeSend();
	 
};	


#endif
