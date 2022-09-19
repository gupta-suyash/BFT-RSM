#include "iothread.h"
#include "pipeline.h"
#include "connect.h"

UInt16 SendThread::GetThreadId()
{
	return thd_id_;
}	

void SendThread::Init(UInt16 thd_id)
{
	last_sent_ = get_node_rsm_id(); 
	thd_id_ = thd_id;
	thd_ = thread(&SendThread::Run, this);
}	

void SendThread::Run() 
{
	//cout << "SndThread: " << GetThreadId() << endl;
	// TODO: Remove this line.
	UInt64 bid=1;

	while(true) {
		// Send to one node in other rsm.
		UInt16 nid = GetLastSent();

		// TODO: Next two lines, remove.
		TestAddBlockToInQueue(bid);
		bid++;

		pipe_ptr->SendToOtherRsm(nid);

		// Set the id of next node to send.
		nid = (nid+1) % get_nodes_rsm();
		SetLastSent(nid);

		//// Broadcast to all in own rsm.
		//pipe_ptr->SendToOwnRsm();
	}
}

UInt16 SendThread::GetLastSent() 
{
	return last_sent_;
}	

void SendThread::SetLastSent(UInt16 id)
{
	last_sent_ = id;
}	


void SendThread::TestAddBlockToInQueue(UInt64 bid) 
{
	string str = "Tmsg " + to_string(bid);
	SendBlock(bid, &str[0]);
}	
	

UInt16 RecvThread::GetThreadId()
{
	return thd_id_;
}

void RecvThread::Init(UInt16 thd_id)
{
	thd_id_ = thd_id;
	thd_ = thread(&RecvThread::Run, this);
}

void RecvThread::Run()
{
	//cout << "RecvThread: " << GetThreadId() << endl;
	while(true) {
		pipe_ptr->RecvFromOtherRsm();
		//pipe_ptr->RecvFromOwnRsm();
	}
}



