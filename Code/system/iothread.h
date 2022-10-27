#ifndef _IO_THREAD_
#define _IO_THREAD_

#include <thread>
#include "global.h"
#include "pipe_queue.h"

class IOThreads {
public:
	UInt16 thd_id_; // Thread id.
	thread thd_;	// Thread.
	virtual void Init(UInt16 thd_id) = 0;
	virtual void Run() = 0;
	virtual UInt16 GetThreadId() = 0;
};	


// Threads that send or receive messages.
class SendThread : public IOThreads {
	UInt16 last_sent_; // Id of node from other RSM.
public:
	//UInt16 thd_id_; // Thread id.
	//thread thd_;	// Thread.
	void Init(UInt16 thd_id);
	void Run();
	UInt16 GetThreadId();
	
	UInt16 GetLastSent();
	void SetLastSent(UInt16 id);

	void TestAddBlockToInQueue(const UInt64 bid);
};	


class RecvThread : public IOThreads {
public:
	//UInt16 thd_id_; // Thread id.
	//thread thd_;	// Thread.
	void Init(UInt16 thd_id);
	void Run();
	UInt16 GetThreadId();
};


#endif
