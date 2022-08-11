#ifndef _IO_THREAD_
#define _IO_THREAD_

#include <thread>
#include "pipeline.h"

class IOThreads {
	vector <thread> ithreads_; // Input (Receive) threads.
	vector <thread> othreads_; // Output (Send) threads.
	unique_ptr<Pipeline> iopipe_;
	//Pipeline *iopipe_;
public:
	IOThreads();
	string GetRecvUrl(UInt16 cnt);
	string GetSendUrl(UInt16 cnt);
	void SetIThreads();
};	

#endif
