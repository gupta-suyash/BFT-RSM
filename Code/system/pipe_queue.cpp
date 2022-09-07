#include "pipe_queue.h"
#include <cstring>

void PipeQueue::Init()
{
	msg_queue_ = new boost::lockfree::queue<DataPack *>(0);
}	

/* Pushes a message to the queue.
 * 
 * @param msg is the message to be queued..
 */
void PipeQueue::Enqueue(unique_ptr<DataPack> msg)
{
	DataPack *q_msg = msg.release();

	// Continue trying to push until successful.
	while(!msg_queue_->push(q_msg));
}	

/* Pops a message from the rear of the queue.
 * 
 * @return the popped message.
 */
std::unique_ptr<DataPack> PipeQueue::Dequeue()
{
	bool valid = false;
	DataPack *msg = new DataPack();

	// Popping the message; valid returns the status.
	valid = msg_queue_->pop(msg);
	if(valid) {
		cout << "Dequeued: " << msg->buf << endl;
	} else {
		msg->data_len = 0;
	}

	return unique_ptr<DataPack>(msg);
}

/* The following functions are meant to test the correctness of the queue.
 *
 */ 
void PipeQueue::CallE()
{
	for(int i=0; i<20; i++) {
		string str = "abc" + to_string(i);
		char *buf = &str[0];

		unique_ptr<DataPack> msg = std::make_unique<DataPack>();
		msg->data_len = strlen(buf) + 1;
		msg->buf = new char[msg->data_len];
		memcpy(msg->buf, buf, msg->data_len);

		cout << "Enqueue: " << msg->buf << endl;

		Enqueue(std::move(msg));
	}
}

void PipeQueue::CallD()
{
	int i=0;
	while(i < 20) {
		unique_ptr<DataPack> msg = Dequeue();
		if(msg->data_len != 0) {
			cout << "Dequeue done: " << msg->buf << endl;
			i++;
		}
	}	
}

void PipeQueue::CallThreads() 
{
	cout << "Calling threads " << endl;
	thread ot = thread(&PipeQueue::CallD, this);
	thread it = thread(&PipeQueue::CallE, this);

	ot.join();
	it.join();
}
