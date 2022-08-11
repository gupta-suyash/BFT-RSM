#include "iothread.h"

IOThreads::IOThreads() 
{
	iopipe_ = make_unique<Pipeline>();
}

string IOThreads::GetRecvUrl(UInt16 cnt) 
{
	UInt16 port_id = get_port_num() + (get_node_id() * get_nodes_rsm()) + cnt;
	auto r = iopipe_.get();
	string url = "tcp://" + r->getIP(get_node_id()) + ":" + to_string(port_id); 
	return url;
}	

string IOThreads::GetSendUrl(UInt16 cnt) 
{
	UInt16 port_id = get_port_num() + (cnt * get_nodes_rsm()) + get_node_id();
	auto r = iopipe_.get();
	string url = "tcp://" + r->getIP(cnt) + ":" + to_string(port_id); 
	return url;
}

void IOThreads::SetIThreads() 
{
	UInt16 port_id = 7000;
	//iopipe_ = new Pipeline();
	for(UInt16 i=0; i<g_node_cnt; i++) {
		string rurl = GetRecvUrl(i);
		const char *recv_url = rurl.c_str();
		//iopipe_->NodeReceive(url);
		auto rptr = iopipe_.get();
		thread it(rptr->NodeReceive, recv_url);
		ithreads_.push_back(move(it));	
		
		// Similar task for othreads;
		string surl = GetSendUrl(i);
		const char *send_url = surl.c_str();
		//iopipe_->NodeSend(url);
		auto sptr = iopipe_.get();
		thread ot(sptr->NodeSend, send_url);
		othreads_.push_back(move(ot));
	}
}	
