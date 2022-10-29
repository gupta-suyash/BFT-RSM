#ifndef _MESSAGE_
#define _MESSAGE_

#include "global.h"

class Message {
	UInt64 msize_;
	UInt64 txn_id_;		// Identifier associated with the message.
	char *data_;		// Message content.
	UInt64 cumm_ack_;	// Cummulative acknowledgement of received messages. 
public:
	static Message * CreateMsg();
	void SetTxnId(UInt64 txn_id);
	void SetAckId(UInt64 cumm_ack_id);
	void SetData(char* data, UInt64 msize);

	void CopyFromBuf(char *buf);
	char * CopyToBuf();
	UInt64 GetSize();

	static void TestFunc();
};	


#endif
