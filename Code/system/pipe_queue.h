#ifndef _PIPE_QUEUE_
#define _PIPE_QUEUE_

#include <iostream>
#include <thread>
#include "global.h"
#include "message.h"


class PipeQueue
{
private:
	boost::lockfree::queue<Message*> *msg_queue_;
	boost::lockfree::queue<Message*> *store_queue_;
public:
	void Init();
	void Enqueue(Message *msg);
	Message* Dequeue();
	Message* EnqueueStore();

	// Msg_Queue Testing functions.
	void CallE();
	void CallD();
	void CallThreads();
};	


#endif
