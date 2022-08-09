#ifndef _IO_THREAD_
#define _IO_THREAD_

#include <thread>
#include "pipeline.h"

class IOThreads {
	vector <thread> ithreads; // Input (Receive) threads.
	vector <thread> othreads; // Output (Send) threads.
public:
	void SetIThreads();
};	

#endif
