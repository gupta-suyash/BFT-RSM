#ifndef _PIPE_QUEUE_
#define _PIPE_QUEUE_

#include <iostream>
#include <thread>
#include "global.h"
#include "data_comm.h"


class PipeQueue
{
	std::queue<crosschain_proto::CrossChainMessage> msg_queue_;
	std::queue<crosschain_proto::CrossChainMessage> store_queue_;
public:
	void Enqueue(crosschain_proto::CrossChainMessage msg);
	crosschain_proto::CrossChainMessage Dequeue();
	crosschain_proto::CrossChainMessage EnqueueStore();

	// Msg_Queue Testing functions.
	void CallE();
	void CallD();
	void CallThreads();
};	


#endif
