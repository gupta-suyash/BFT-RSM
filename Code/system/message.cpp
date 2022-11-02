#include "message.h"

Message * Message::CreateMsg()
{
	Message *msg = new Message();
	msg->msize_ = UINT64_MAX;
	msg->txn_id_ = 0;
	msg->cumm_ack_ = UINT64_MAX;

	return msg;
}

UInt64 Message::GetTxnId()
{
	return txn_id_;
}

void Message::SetTxnId(UInt64 txn_id)
{
	txn_id_ = txn_id;
}	

UInt64 Message::GetAckId()
{
	return cumm_ack_;
}

void Message::SetAckId(UInt64 cumm_ack_id)
{
	cumm_ack_ = cumm_ack_id;
}	

char * Message::GetData()
{
	return data_;
}

void Message::SetData(char* data, UInt64 msize) 
{
	msize_ = msize;
	data_ = new char[msize_];
	memcpy(data_, data, msize);
	//data_ = data;
}	

void Message::CopyFromBuf(char *buf)
{
	//UInt64 ptr = 0;
	//// Copying the first field: size of the data blob.
	//COPY_VAL(msize_, buf, ptr);
	//// Copying next field, txn_id_
	//COPY_VAL(txn_id_, buf, ptr);
	//// Copying next field, cummulative ack 
	//COPY_VAL(cumm_ack_, buf, ptr);
	//data_ = new char[msize_+1];
	//COPY_VAL(data_, buf, ptr);

	// String from buf.
	string str = std::string(buf);
	cout << "MString: " << str << " :: " << str.length() << endl;

	size_t pos = 0;
	std::string str_sub;
	std::string delimit = " ";

	// Extracting data blob size.
	pos = str.find(delimit);
	str_sub = str.substr(0, pos);
	msize_ = std::stoi(str_sub);
	str.erase(0, pos + delimit.length());

	// Extracting data blob.
	pos = str.find(delimit);
	str_sub = str.substr(0, pos);
	data_ = new char[msize_];
	memcpy(data_, &str_sub[0], msize_);
	str.erase(0, pos + delimit.length());

	// Extracting txn_id.
	txn_id_ = std::stoi(str);
	
	cout << "Data: " << data_ << " :: Blob size: " << msize_ << endl;
}

	

char * Message::CopyToBuf()
{
	// Getting the size of the message.
	//UInt64 sz = GetSize();
	//char *buf = new char[sz];
	//COPY_BUF(buf, msize_, ptr);
	//COPY_BUF(buf, txn_id_, ptr);
	//COPY_BUF(buf, cumm_ack_, ptr);
	//COPY_BUF(buf, data_, ptr);

	string dstr = std::string(data_);
	string str = to_string(dstr.length()) + " " + dstr + " ";
	str += to_string(txn_id_);
	SetStringSize(str.length());
	//cout << "str: " << str << endl;

	char *buf = &str[0];
	cout << "Buffer: " << buf << " : " << GetStringSize() << endl;

	return buf;
}	

UInt64 Message::GetSize()
{
	UInt64 sz = 0;
	sz += sizeof(UInt64);
	//sz += sizeof(UInt64);
	//sz += sizeof(UInt64);
	sz += msize_+1;

	return sz;
}	

UInt64 Message::GetStringSize()
{
	return str_size_;
}	

void Message::SetStringSize(UInt64 sz)
{
	str_size_ = sz;
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


	
