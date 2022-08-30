#include "pipe_queue.h"

/* Initialize the lockfree queues for sender threads; one queue per 
 * node in the other RSM.
 */ 
void SendPipeQueue::Init() 
{
	// We have a set of sender queues, one per node.
	snd_queue = new boost::lockfree::queue<DataPack*> *[2]; //[get_nodes_rsm()];
	for(int i=0; i<2; i++) {
		snd_queue[i] = new boost::lockfree::queue<DataPack*>(1);
	}
}


/* Pushes a message to the queue for a specific sender thread.
 * 
 * @param buf is the message to be sent.
 */
void SendPipeQueue::Enqueue(char *buf, size_t data_len)
{
	unique_ptr<DataPack> msg = std::make_unique<DataPack>();
	msg->data_len = data_len;
	msg->buf = new char[msg->data_len];
	memcpy(msg->buf, buf, msg->data_len);
	DataPack *q_msg = msg.release();

	//// We convert the buf to a Message structure.
	//Message *msg = Message::CreateMsg(buf, kPipeQueue);

	// Determining which sender thread's queue to place the message.
	UInt64 qid = 0; //msg->txn_id % get_nodes_rsm();

	// Continue trying to push until successful.
	while(!snd_queue[0]->push(q_msg));

	// Remember to delete buf at the caller as we have created a copy.
}	

/* Each sender thread pops a message from the rear of itsqueue.
 * 
 * @return the popped message.
 */
std::unique_ptr<DataPack> SendPipeQueue::Dequeue(UInt16 thd_id)
{
	bool valid = false;
	DataPack *msg = NULL;

	// Popping the message; valid returns the status.
	int i=0;
	while(true) {
		valid = snd_queue[0]->pop(msg);
		if(valid) {
			cout << "Dequeued: " << msg->buf << endl;
			i++;
		}
		if(i >= 10)
			break;
	}
	return std::unique_ptr<DataPack>(msg);
}


void SendPipeQueue::CallE()
{
	string str = "abc";
	char *msg;

	for(int i=0; i<10; i++) {
		str = "abc" + to_string(i);
		msg = &str[0];
		size_t sz = strlen(msg) + 1;

		cout << "Enqueue: " << msg << endl;
		Enqueue(msg, sz);
	}
}	

void SendPipeQueue::CallD()
{
	unique_ptr<DataPack> msg = Dequeue(0);
	cout << "Dequeue done: " << msg->buf << endl;

}

void SendPipeQueue::CallThreads() 
{
	Init();

	thread it = thread(&SendPipeQueue::CallE, this);
	thread ot = thread(&SendPipeQueue::CallD, this);

	it.join();
	ot.join();
}	


/* // This is how we expect these functions to be accessed by some thread.
 * // First, a pointer to this class will be declared in global.h
 * unique_ptr<SendPipeQueue> sp_queue = make_unique<SendPipeQueue>();
 *
 * // Next two tasks will happen in main.cpp as queue init by a single thread.
 * auto sp_raw = sp_queue.get();
 * sp_raw->init(); // Initialize queues
 *
 * // Now, the data that we want to send to other RSM is received through 
 * // some socket connection or API from the host node -- Need to decide this.
 * // Whenever we get the data, we will get it in the form char *buf. 
 * // So, we need to enqueue it in the queue for send threads to pick up.
 * sp_raw->enqueue(buf);
 *
 * // Somewherer in the definition of the send threads we would need to dequeue.
 * Message *msg = sp_raw->dequeue();
 *
 */ 
