#ifndef _ACKNOW_
#define _ACKNOW_

#include <list>
#include <mutex>
#include <unordered_map>

#include "global.h"

class Acknowledgment
{
	UInt64 ackValue;
	std::mutex ack_mutex;
	std::list<UInt64> msg_recv_;

public:
	void Init();
	void AddToAckList(UInt64 mid);
	UInt64 GetAckIterator();

	void TestFunc();
};	


class QuorumAcknowledgment
{
	UInt64 quack_value_;
	std::unordered_map<UInt64, UInt16> quack_recv_;

public:
	void Init();
	void QuackCheck(UInt64 min, UInt64 max);
	void AddToQuackMap(UInt64 mid);
	UInt64 GetQuackIterator();

	void TestFunc();
};

#endif
