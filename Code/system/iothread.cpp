#include "iothread.h"

void SndThread::Init(UInt16 thd_id)
{
	thd_id_ = thd_id;
	//thread abc = thread(&SndThread::RunYo, this);
	//abc.join();

	thd_ = thread(&SndThread::RunYo, this);
	//thd_.join();
}	

void SndThread::RunYo() 
{
	cout << "Hello" << endl;
}

void RcvThread::Run()
{
	cout << "Hello" << endl;
}	
