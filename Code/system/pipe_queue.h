#ifndef _PIPE_QUEUE_
#define _PIPE_QUEUE_

#include <iostream>
#include "global.h"

#include <boost/lockfree/spsc_queue.hpp>

class Message;

class PipeQueue
{
public:
	virtual void Init() = 0;
	virtual void Enqueue(char *buf) = 0;
	virtual Message *Dequeue() = 0;
};	


class SendPipeQueue : public PipeQueue {
	boost::lockfree::queue<Message*> **snd_queue;
public:
	void Init();
	void Enqueue(char *buf);
	Message *Dequeue();
};

#endif
