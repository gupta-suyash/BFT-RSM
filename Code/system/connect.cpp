#include "connect.h"
#include "global.h"
#include "message.h"
#include <boost/lockfree/queue.hpp>

void InitInQueue()
{
	in_queue = new boost::lockfree::queue<Message *>(0);	
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
void SendBlock(uint64_t block_id, char *block, size_t bsize) 
{
	unique_ptr<Message> pmsg = make_unique<Message>();
	Message *msg = pmsg.release();

	msg->SetTxnId(block_id);
	msg->SetData(block, bsize);

	cout << "Message added InQueue: " << msg->GetTxnId() << " : Data: " << msg->GetData() << endl;

	while(!in_queue->push(msg));
}

void ReceiveBlock()
{

}	
