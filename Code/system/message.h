#ifndef _MESSAGE_
#define _MESSAGE_

#include "global.h"

class Message {
public:
	UInt64 msize_;		// Size of the message.
	MessageType mtype_;	// Tyep of message.
	UInt64 txn_id_;		// Identifier associated with the message.
	char *data_;	
	UInt16 sender_id_;	// Sending node's identifier.
	UInt16 receiver_id_;	// Destination node's identifier.
	UInt64 cumm_ack_;	// Cummulative acknowledgement of received messages. 

	static Message * CreateMsg(char *buf, UInt64 sz, MessageType mtype);
	static Message * CreateMsg(char *buf, MessageType mtype);
	static Message * CreateMsg(MessageType mtype);

	virtual void CopyFromBuf(char *buf, UInt64 sz) = 0;
	virtual void CopyFromBuf(char *buf) = 0;
	virtual char * CopyToBuf() = 0;
	virtual UInt64 GetSize() = 0;
};	


class SendMessage : public Message {
public:
	void CopyFromBuf(char *buf, UInt64 sz);
	void CopyFromBuf(char *buf);
	char * CopyToBuf();
	UInt64 GetSize();

	static void TestFunc();
};	


#endif
