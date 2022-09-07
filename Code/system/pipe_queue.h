#ifndef _PIPE_QUEUE_
#define _PIPE_QUEUE_

#include <iostream>
#include <thread>

#include "global.h"
#include "data_comm.h"

#include <boost/lockfree/queue.hpp>


class PipeQueue
{
	boost::lockfree::queue<DataPack*> *msg_queue_;
public:
	void Init();
	void Enqueue(unique_ptr<DataPack> msg);
	std::unique_ptr<DataPack> Dequeue();

	// Queue Testing functions.
	void CallE();
	void CallD();
	void CallThreads();
};	


#endif
