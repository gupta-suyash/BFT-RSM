#ifndef _MESSAGE_
#define _MESSAGE_

#include "global.h"

class Message {
	UInt64 msize_;
	UInt64 txn_id_;		// Identifier associated with the message.
	char *data_;		// Message content.
	UInt64 cumm_ack_;	// Cummulative acknowledgement of received messages. 
	UInt64 str_size_;	// Size of string equivalent of this message.
public:
	static Message * CreateMsg();
	UInt64 GetTxnId();
	void SetTxnId(UInt64 txn_id);
	UInt64 GetAckId();
	void SetAckId(UInt64 cumm_ack_id);
	char * GetData();
	void SetData(char* data, UInt64 msize);
	UInt64 GetStringSize();
	void SetStringSize(UInt64 sz);

	void CopyFromBuf(char *buf);
	char * CopyToBuf();
	UInt64 GetSize();

	static void TestFunc();
};	


#endif
