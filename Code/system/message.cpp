#include "message.h"

Message * Message::CreateMsg()
{
	Message *msg = new Message();
	msg->msize_ = UINT64_MAX;
	msg->txn_id_ = UINT64_MAX;
	msg->cumm_ack_ = UINT64_MAX;

	return msg;
}

void Message::SetTxnId(UInt64 txn_id)
{
	txn_id_ = txn_id;
}	

void Message::SetAckId(UInt64 cumm_ack_id)
{
	cumm_ack_ = cumm_ack_id;
}	

void Message::SetData(char* data, UInt64 msize) 
{
	msize_ = msize;
	data_ = data;
}	

void Message::CopyFromBuf(char *buf)
{
	UInt64 ptr = 0;

	// Copying the first field: size of the data blob.
	COPY_VAL(msize_, buf, ptr);

	// Copying next field, txn_id_
	COPY_VAL(txn_id_, buf, ptr);

	// Copying next field, cummulative ack 
	COPY_VAL(cumm_ack_, buf, ptr);

	// Copying the rest of the messages as the data blob.
	data_ = new char[msize_];
	COPY_VAL(data_, buf, ptr);

	cout << "Data Blob Size: " << msize_ << endl;
	cout << "Data Blob: " << data_ << endl;

	//// Copying the buf as the rest of the message; data blob.
	//data_ = new char[sz];
	//memcpy(data_, buf, sz);
}

	

char * Message::CopyToBuf()
{
	// Getting the size of the message.
	UInt64 sz = GetSize();

	char *buf = new char[sz];

	UInt64 ptr = 0;
	COPY_BUF(buf, msize_, ptr);
	COPY_BUF(buf, txn_id_, ptr);
	COPY_BUF(buf, cumm_ack_, ptr);
	COPY_BUF(buf, data_, ptr);

	return buf;
}	

UInt64 Message::GetSize()
{
	UInt64 sz = 0;
	sz += sizeof(UInt64);
	sz += sizeof(UInt64);
	sz += sizeof(UInt64);
	sz += msize_;

	return sz;
}	


void Message::TestFunc()
{
	string str = "Hello";
	char *c_str = &str[0];
	UInt64 sz = strlen(c_str)+1;
	
	Message *msg1 = Message::CreateMsg();
	msg1->SetTxnId(3);
	msg1->SetAckId(112);

	msg1->SetData(c_str, sz);
	cout << "Checking: " << msg1->msize_ << " :: " << msg1->data_ << endl;

	char *buf = msg1->CopyToBuf();

	Message *msg2 = Message::CreateMsg();
	msg2->CopyFromBuf(buf);
	cout << "Again: " << msg2->msize_ << " :: " << msg2->data_ << endl;
}	


	
