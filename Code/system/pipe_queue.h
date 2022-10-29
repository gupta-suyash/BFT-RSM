#ifndef _PIPE_QUEUE_
#define _PIPE_QUEUE_

#include <iostream>
#include <thread>
#include <mutex>
#include "global.h"
#include "data_comm.h"


class PipeQueue
{
private:
	std::mutex msg_q_mutex;
	std::queue<crosschain_proto::CrossChainMessage> msg_queue_;
	std::mutex store_q_mutex;
	std::queue<crosschain_proto::CrossChainMessage> store_queue_;
	//boost::lockfree::queue<DataPack*> *msg_queue_;
	//boost::lockfree::queue<ProtoMessage*> *store_queue_;
public:
	void Enqueue(crosschain_proto::CrossChainMessage msg);
	crosschain_proto::CrossChainMessage Dequeue();
	crosschain_proto::CrossChainMessage EnqueueStore();
	//void Enqueue(Message *msg);
	//Message* Dequeue();
	//Message* EnqueueStore();

	// Msg_Queue Testing functions.
	void CallE();
	void CallD();
	void CallThreads();
};	


#endif
