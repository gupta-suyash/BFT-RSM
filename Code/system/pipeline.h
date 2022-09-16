#ifndef _PIPELINE_
#define _PIPELINE_

#include <iostream>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <thread>
#include <map>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>

#include "nng/nng.h"
#include <nng/protocol/pipeline0/pull.h>
#include <nng/protocol/pipeline0/push.h>

#include "global.h"
#include "data_comm.h"

#include "crosschainmessage.pb.h"
using std::filesystem::current_path;

#define NODE0 "node0"
#define NODE1 "node1"


class Pipeline {
	vector<string> ip_addr; // IP addresses of own RSM.
	//vector <string> tcp_url;
	vector <thread> athreads_; // Input (Receive) threads.
	
	std::map <UInt16, nng_socket> send_sockets_;
	std::map <UInt16, nng_socket> recv_sockets_;

public:			    
	Pipeline();
	string GetPath();
	void ReadIfconfig(string if_path);
	string getIP(UInt16 id); 

	string GetRecvUrl(UInt16 cnt);
	string GetSendUrl(UInt16 cnt);
	void SetSockets();

	void SendToOtherRsm(UInt16 nid);
	void RecvFromOtherRsm();

	void SendToOwnRsm();
	void RecvFromOwnRsm();

	void DataSend(crosschain_proto::CrossChainMessage* buf, UInt16 node_id);
	unique_ptr<DataPack> DataRecv(UInt16 node_id);

	char *DeepCopyMsg(char *msg);
	

	//char* DataToHost();
	//void DataFromHost(char *buf);
};	


#endif
