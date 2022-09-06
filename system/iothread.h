#ifndef _IO_THREAD_
#define _IO_THREAD_

#include <thread>
#include "global.h"
#include "pipe_queue.h"

class IOThreads {
public:
	UInt16 thd_id_;
	thread thd_;
	virtual void Init(UInt16 thd_id) = 0;
	virtual void Run() = 0;
	virtual UInt16 GetThreadId() = 0;
};	


class InterSndThread : public IOThreads {
public:
	void Init(UInt16 thd_id);
	void Run();
	UInt16 GetThreadId();
};	


class InterRcvThread : public IOThreads {
public:
	void Init(UInt16 thd_id);
	void Run();
	UInt16 GetThreadId();
};


#endif
