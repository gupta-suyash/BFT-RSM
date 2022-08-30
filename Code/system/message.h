#ifndef _MESSAGE_
#define _MESSAGE_

#include "global.h"

class Message {
public:
	MessageType mtype_;
	UInt64 txn_id_;
	char *data_;
	UInt16 sender_id_;
	UInt16 receiver_id_;
	

	static Message * CreateMsg(char *buf, MessageType mtype);
	static Message * CreateMsg(MessageType mtype);

	virtual UInt64 GetSize() = 0;
	virtual void Init() = 0;
};	


//class PipeQueueMessage : public Message {
//public:
//	UInt64 GetSize();
//	void Init();
//};	

#endif
