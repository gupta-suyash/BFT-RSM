#include "iothread.h"
#include "pipeline.h"

UInt16 SendThread::GetThreadId()
{
	return thd_id_;
}	

void SendThread::Init(UInt16 thd_id)
{
	thd_id_ = thd_id;
	thd_ = thread(&SendThread::Run, this);
}	

void SendThread::Run() 
{
	cout << "SndThread: " << GetThreadId() << endl;
	pipe_ptr->RunSend();
}

	

UInt16 RecvThread::GetThreadId()
{
	return thd_id_;
}

void RecvThread::Init(UInt16 thd_id)
{
	thd_id_ = thd_id;
	thd_ = thread(&RecvThread::Run, this);
}

void RecvThread::Run()
{
	cout << "RecvThread: " << GetThreadId() << endl;
	pipe_ptr->RunRecv();
}



