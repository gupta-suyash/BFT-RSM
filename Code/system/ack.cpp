#include "ack.h"

/* Appends max integer to the list and ackValue is set to this max integer.
 *
 */ 
void Acknowledgment::Init()
{
	// The first entry to the list is not a msg identifier (max int).
	msg_recv_.push_back(MAX_UINT64);
	ackValue = MAX_UINT64;
}

/* Adds an element to the lsit msg_recv_. 
 * We also do a sliding window style operation where consecutive acks are deleted.
 *
 * @param mid is the value to be added
 */
void Acknowledgment::AddToAckList(UInt64 mid)
{
	cout << "AckValue: " << ackValue << " :: mid: " << mid << endl;

	auto it = msg_recv_.begin();

	// If the first element is max value, remove it and add mid.
	if(*it == MAX_UINT64) {
		msg_recv_.push_back(mid);
		it = msg_recv_.erase(it);
	} else {
		// Flag to check if mid is added at the end of the list.
		bool last_flag = true;
		for(; it != msg_recv_.end(); ++it) {
			cout << "Compare Value: " << *it << endl;
			if(*it > mid) {
				// Insert before the element larger than mid.
				msg_recv_.emplace(it, mid);
				last_flag = false;
				break;
			}
		}

		if(last_flag) {
			// Appending to the list.
			msg_recv_.push_back(mid);
		}	
	}	

	// If mid = 0, set the ackValue to 0.
	if(mid == 0) {
		ackValue = 0;
	}

	// Enter if ackValue is not MAX_UINT64; possible if mid=0, not inserted yet.
	if(ackValue != MAX_UINT64) {
		// Flag to determine if we need to delete old acknowledgments.
		bool del_flag = false;
		it = msg_recv_.begin(); 
		it++;
		for(;it != msg_recv_.end(); it++) {
			if(*it != ackValue+1) {
				break;
			}
			ackValue++;
			del_flag = true;
		}

		if(del_flag) {
			// Delete from beginning to the element less than ackValue.
			--it;
			msg_recv_.erase(msg_recv_.begin(), it);
		}	
	}	
}

/* Get the value of variable ackValue; needs to be locked as multi-threaded access.
 *
 * @return ackValue
 */ 
UInt64 Acknowledgment::GetAckIterator()
{
	return ackValue;
}	


/* This function is just to test the functionality in a single-threaded execution.
 *
 */
void Acknowledgment::TestFunc() 
{
	/*
	 * Test 1
	 *
	AddToAckList(0);
	cout << "AckValue is: " << GetAckIterator() << " :: First element: " << *(msg_recv_.begin()) << " :: Size: " << msg_recv_.size() << endl;

	AddToAckList(1);
	cout << "AckValue is: " << GetAckIterator() << " :: First element: " << *(msg_recv_.begin()) << " :: Size: " << msg_recv_.size() << endl;
	
	AddToAckList(10);
	cout << "AckValue is: " << GetAckIterator() << " :: First element: " << *(msg_recv_.begin()) << " :: Size: " << msg_recv_.size() << endl;


	AddToAckList(11);
	cout << "AckValue is: " << GetAckIterator() << " :: First element: " << *(msg_recv_.begin()) << " :: Size: " << msg_recv_.size() << endl;

	AddToAckList(4);
	cout << "AckValue is: " << GetAckIterator() << " :: First element: " << *(msg_recv_.begin()) << " :: Size: " << msg_recv_.size() << endl;
	
	AddToAckList(3);
	cout << "AckValue is: " << GetAckIterator() << " :: First element: " << *(msg_recv_.begin()) << " :: Size: " << msg_recv_.size() << endl;	

	AddToAckList(2);
	cout << "AckValue is: " << GetAckIterator() << " :: First element: " << *(msg_recv_.begin()) << " :: Size: " << msg_recv_.size() << endl;
	*/

	/* 
	 * Test 2
	 *
	AddToAckList(0);
	cout << "AckValue is: " << GetAckIterator() << " :: First element: " << *(msg_recv_.begin()) << " :: Size: " << msg_recv_.size() << endl;

	AddToAckList(10);
	cout << "AckValue is: " << GetAckIterator() << " :: First element: " << *(msg_recv_.begin()) << " :: Size: " << msg_recv_.size() << endl;

	AddToAckList(11);
	cout << "AckValue is: " << GetAckIterator() << " :: First element: " << *(msg_recv_.begin()) << " :: Size: " << msg_recv_.size() << endl;

	AddToAckList(1);
	cout << "AckValue is: " << GetAckIterator() << " :: First element: " << *(msg_recv_.begin()) << " :: Size: " << msg_recv_.size() << endl;
	*/


	/*
	 * Test 3
	 */ 
	AddToAckList(0);
	cout << "AckValue is: " << GetAckIterator() << " :: First element: " << *(msg_recv_.begin()) << " :: Size: " << msg_recv_.size() << endl;

	AddToAckList(2);
	cout << "AckValue is: " << GetAckIterator() << " :: First element: " << *(msg_recv_.begin()) << " :: Size: " << msg_recv_.size() << endl;

	AddToAckList(3);
	cout << "AckValue is: " << GetAckIterator() << " :: First element: " << *(msg_recv_.begin()) << " :: Size: " << msg_recv_.size() << endl;

	AddToAckList(1);
	cout << "AckValue is: " << GetAckIterator() << " :: First element: " << *(msg_recv_.begin()) << " :: Size: " << msg_recv_.size() << endl;
}	
