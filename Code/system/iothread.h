#ifndef _IO_THREAD_
#define _IO_THREAD_

#include <thread>
#include "pipeline.h"

class IOThreads {
	
	unique_ptr<Pipeline> iopipe_;
	//Pipeline *iopipe_;
};	

#endif
