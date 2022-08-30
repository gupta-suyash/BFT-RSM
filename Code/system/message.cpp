#include "message.h"

Message * Message::CreateMsg(char *buf, MessageType mtype)
{
	//Message *msg = CreateMsg(mtype);
	Message *msg = NULL;
	//COPY_VAL(dest_id,data,ptr);
	return msg;
}	

Message * Message::CreateMsg(MessageType mtype)
{
	Message *msg;
	//switch(mtype) {
	//	case kPipeQueue:
	//		msg = new PipeQueueMessage;
	//		break;
	//	default:
	//		cout << "Invalid Type" << endl;
	//		exit(1);
	//}
	msg->mtype_ = mtype;
	msg->txn_id_ = UINT64_MAX;
	msg->sender_id_ = UINT16_MAX;
	msg->receiver_id_ = UINT16_MAX;
	return msg;
}	
