#include "iothread.h"

void IOThreads::SetIThreads() 
{
	const char *url = "abc";
	UInt16 port_id = 7000;
	//iopipe_ = make_unique<Pipeline>();
	iopipe_ = new Pipeline();
	for(UInt16 i=0; i<g_node_cnt; i++) {
		//url = get_url(port_id);
		// Need to get correct port id for every node.

		iopipe_->NodeReceive(url);
		//auto r = iopipe_.get();
		thread it(iopipe_->Pipeline::NodeReceive, url);
		//ithreads_.push_back(move(it));	
		
		// Similar task for othreads;
		//iopipe_->NodeSend(url);
		//thread ot(iopipe_->Nodesend, url);
		//othreads_.push_back(move(it));
	}
}	
