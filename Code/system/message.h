#ifndef _PIPELINE_
#define _PIPELINE_

#include "global.h"

class Message 
{
public:
	UInt64 txn_id;

	static Message * CreateMsg(Char *buf, MessageType rtype);
};	


class PipeQueueMessage : public Message
{
	
}	

#endif
