#include "iothread.h"

UInt16 InterSndThread::GetThreadId()
{
	return thd_id_;
}	

void InterSndThread::Init(UInt16 thd_id)
{
	thd_id_ = thd_id;
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


UInt16 IntraSndThread::GetThreadId()
{
	return thd_id_;
}

void IntraSndThread::Init(UInt16 thd_id)
{
	thd_id_ = thd_id;
	thd_ = thread(&IntraSndThread::Run, this);
}

void IntraSndThread::Run()
{
	cout << "IntraSndThread" << endl;
	sp_qptr->CallD();
}


UInt16 IntraRcvThread::GetThreadId()
{
	return thd_id_;
}

void IntraRcvThread::Init(UInt16 thd_id)
{
	thd_id_ = thd_id;
	thd_ = thread(&IntraRcvThread::Run, this);
}

void IntraRcvThread::Run()
{
	cout << "IntraRcvThread" << endl;
	sp_qptr->CallD();
}
