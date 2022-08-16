#ifndef _IO_THREAD_
#define _IO_THREAD_

#include <thread>
#include "global.h"
#include "pipeline.h"

class IOThreads {
public:
	UInt16 thd_id_;
	thread thd_;
	virtual void Init(UInt16 thd_id) = 0;
	virtual void RunYo() = 0;
};	


class SndThread : public IOThreads {
public:
	void Init(UInt16 thd_id);
	void RunYo();
};	


class RcvThread : public IOThreads {
public:
	void Init(UInt16 thd_id);
	void Run();
};


#endif
