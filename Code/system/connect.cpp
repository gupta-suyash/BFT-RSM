#include "connect.h"
#include "global.h"
#include "data_comm.h"

#include <boost/lockfree/queue.hpp>

void Init()
{
	in_queue = new boost::lockfree::queue<ProtoMessage *>(0);
}	

/* The protocol running at the node will call this function to send a block
 * to the Scrooge library. Any received block will be placed in a lockfree 
 * queue, which can be accessed by Scrooge threads.
 *
 * We do not expect Scrooge threads to access these functions. Only the protocol
 * threads (such as Algorand) will call these functions and enqueue block to the 
 * queue.
 *
 * @params undefined TODO.
 */ 
void SendBlock(uint64_t block_id, char *block) 
{
	ProtoMessage *msg = ProtoMessage::SetMessage(block_id, block);
	while(!in_queue->push(msg));
	
	// TODO: Do we need to delete this msg? 
}

void ReceiveBlock()
{

}	
