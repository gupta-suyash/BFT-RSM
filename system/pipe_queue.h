#ifndef _PIPE_QUEUE_
#define _PIPE_QUEUE_

#include <iostream>
#include <thread>

#include "global.h"
#include "data_comm.h"
#include "message.h"

#include <boost/lockfree/queue.hpp>


class PipeQueue
{
public:
	virtual void Init() = 0;
	virtual void Enqueue(char *buf, size_t data_len, int qid) = 0;
	virtual std::unique_ptr<DataPack> Dequeue(UInt16 thd_id) = 0;
};	


class SendPipeQueue : public PipeQueue {
	//boost::lockfree::queue<DataPack*> snd_queue{1};
	boost::lockfree::queue<DataPack*> **snd_queue;
public:
	void Init();
	void Enqueue(char *buf, size_t data_len, int qid);
	std::unique_ptr<DataPack> Dequeue(UInt16 thd_id);

	void CallE();
	void CallD();
	void CallThreads();
};

#endif
