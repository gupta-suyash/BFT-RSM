#include "pipe_queue.h"
#include <cstring>

void PipeQueue::Init()
{
	msg_queue_ = new boost::lockfree::queue<Message *>(0);
	store_queue_ = new boost::lockfree::queue<Message *>(0);
}


/* Pushes a message to the queue.
 * 
 * @param msg is the message to be queued..
 */
void PipeQueue::Enqueue(Message *msg)
{
	// Continue trying to push until successful.
	while(!msg_queue_->push(msg));
}	

/* Pops a message from the rear of the queue.
 * 
 * @return the popped message.
 */
Message * PipeQueue::Dequeue()
{
	bool valid = false;
	Message *msg = Message::CreateMsg(); // Is allocation a memory leak.

	// Popping the message; valid returns the status.
	valid = msg_queue_->pop(msg);
	if(valid) {
		cout << "Dequeued: " << msg->GetTxnId() << endl;
	}

	return msg;
}


/* This function is used to dequeue a message received from the protocol running 
 * at the node and forward it to the other RSM iff the node is designated to 
 * forward this message. Nodes select which message to send based on the mod of
 * txn_id_ and number of nodes in each RSM.
 *
 * @return the block to be forwarded.
 */ 
Message * PipeQueue::EnqueueStore()
{
	bool valid = false;
	//unique_ptr<Message> pmsg = make_unique<Message>();
	//Message *msg = pmsg.release();

	Message *msg = Message::CreateMsg(); // TODO: Is allocation a memory leak.

	// Popping out the message from in_queue to send to other RSM.
	valid = in_queue->pop(msg);
	if(valid) {
		cout << "Popped InQueue: " << msg->GetTxnId() << " : Data: " << msg->GetData() << endl;
		if(msg->GetTxnId() % get_nodes_rsm() != get_node_rsm_id()) {
			// Any message that is not supposed to be sent by this node,
			// it pushes it to the store_queue.
			
			while(!store_queue_->push(msg));
			
			// TODO: Is this redundant?
			msg = Message::CreateMsg();
		}	
		
	}

	return msg;	
}	


/* The following functions are meant to test the correctness of the msg_queue.
 *
 */ 
void PipeQueue::CallE()
{
	//for(int i=0; i<20; i++) {
	//	string str = "abc" + to_string(i);
	//	char *buf = &str[0];

	//	unique_ptr<DataPack> msg = std::make_unique<DataPack>();
	//	msg->data_len = strlen(buf) + 1;
	//	msg->buf = new char[msg->data_len];
	//	memcpy(msg->buf, buf, msg->data_len);

	//	cout << "Enqueue: " << msg->buf << endl;

	//	Enqueue(std::move(msg));
	//}
}

void PipeQueue::CallD()
{
	//int i=0;
	//while(i < 20) {
	//	unique_ptr<DataPack> msg = Dequeue();
	//	if(msg->data_len != 0) {
	//		cout << "Dequeue done: " << msg->buf << endl;
	//		i++;
	//	}
	//}	
}

void PipeQueue::CallThreads() 
{
	cout << "Calling threads " << endl;
	thread ot = thread(&PipeQueue::CallD, this);
	thread it = thread(&PipeQueue::CallE, this);

	ot.join();
	it.join();
}
