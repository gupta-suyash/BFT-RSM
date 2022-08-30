#include "iothread.h"

UInt16 InterSndThread::GetThreadId()
{
	return thd_id_;
}	

void InterSndThread::Init(UInt16 thd_id)
{
	thd_id_ = thd_id;
	//thread abc = thread(&SndThread::RunYo, this);
	//abc.join();

	thd_ = thread(&InterSndThread::Run, this);
}	

void InterSndThread::Run() 
{
	cout << "InterSndThread" << endl;
	sp_qptr->CallE();
}

	

UInt16 InterRcvThread::GetThreadId()
{
	return thd_id_;
}

void InterRcvThread::Init(UInt16 thd_id)
{
	thd_id_ = thd_id;
	thd_ = thread(&InterRcvThread::Run, this);
}

void InterRcvThread::Run()
{
	cout << "InterRcvThread" << endl;
	sp_qptr->CallD();
}
