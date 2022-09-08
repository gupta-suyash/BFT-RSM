#ifndef _ACKNOW_
#define _ACKNOW_

#include <list>
#include "global.h"

class Acknowledgment
{
	UInt64 ackValue;
	std::list<UInt64> msg_recv_;

public:
	void Init();
	void AddToAckList(UInt64 mid);
	UInt64 GetAckIterator();

	void TestFunc();
};	

#endif
