#include "pipe_queue.h"
#include <cstring>

PipeQueue::PipeQueue(double wait_time) 
{
	duration = std::chrono::duration<double>(wait_time);
}

/* Pushes a message to the queue.
 *
 * @param msg is the message to be queued..
 */
void PipeQueue::Enqueue(scrooge::CrossChainMessage msg)
{
    // Locking access to queue.
    msg_q_mutex.lock();

    // Continue trying to push until successful.
    msg_queue_.push(msg);

    // Unlocking the mutex.
    msg_q_mutex.unlock();
}

/* Pops a message from the rear of the queue.
 *
 * @return the popped message.
 */
scrooge::CrossChainMessage PipeQueue::Dequeue()
{
    scrooge::CrossChainMessage msg;

    // Locking access to queue.
    msg_q_mutex.lock();

    // Popping the message; valid returns the status.
    // valid = msg_queue_.pop(msg);
    if (!msg_queue_.empty())
    {
        msg = msg_queue_.front();
        msg_queue_.pop();
        cout << "Dequeued: " << msg.data().sequence_number() << endl;
    }
    else
    {
        msg.mutable_data()->set_sequence_number(0);
        ;
    }

    // Unlocking the mutex.
    msg_q_mutex.unlock();

    return msg;
}

/* This function is used to dequeue a message received from the protocol running
 * at the node and forward it to the other RSM iff the node is designated to
 * forward this message. Nodes select which message to send based on the mod of
 * block_id and number of nodes in each RSM.
 *
 * @return the block to be forwarded.
 */
scrooge::CrossChainMessage PipeQueue::EnqueueStore()
{
    scrooge::CrossChainMessage msg;
    // Popping out the message from in_queue to send to other RSM.
    // valid = in_queue->pop(msg);

    if (!in_queue.empty())
    {
        msg = in_queue.front();
        in_queue.pop();
        if (msg.data().sequence_number() % get_nodes_rsm() != get_node_rsm_id())
        {
            // Any message that is not supposed to be sent by this node,
            // it pushes it to the store_queue.
            // cout << "Will store: " << msg.data().sequence_number() << " :: " << msg.data().message_content() << endl;

            store_deque_.push_back(std::make_tuple(msg, std::chrono::steady_clock::now()));

            // TODO: Do we need this or this is extra memory alloc.
            msg.mutable_data()->clear_sequence_number();
            msg.clear_ack_count();
            msg.mutable_data()->clear_message_content();
            msg.mutable_data()->set_sequence_number(0);
        }
    }
    else
    {
        // No message in the queue.
        msg.mutable_data()->set_sequence_number(0);
    }
    return msg;
}

void PipeQueue::DequeueStore(scrooge::CrossChainMessage msg) {
	/*scrooge::CrossChainMessage front = std::get<0>(store_queue_.front());
	if(std::get<0>(store_queue_.front()).data().sequence_number() == msg.data().sequence_number()) {
		store_queue_.pop();
	}
	return front; */// what exactly should this value do?
}


/** This function is used to check how much time has elapsed since the message was supposed
 * to be sent. It only has three possible return values: 1 (the wait time for the message has
 * elapsed), 0 (the wait time has not elapsed), -1 (error: sequence id not found)
 */
void PipeQueue::UpdateStore() {
	const std::lock_guard<std::mutex> lock(store_q_mutex);
	auto it = store_deque_.begin();
	while (it != store_deque_.end()) {
		auto timestamp = std::chrono::steady_clock::now();
		auto curr_duration = std::chrono::steady_clock::now() - std::get<1>(*it);
		if (curr_duration.count() >= duration.count() ? 1 : 0) {
			// TODO: reassign packet to another node
			std::get<1>(*it) = timestamp;
			// TODO: add debugging statement here: SPD_LOG();
		}
		it++;
	}
}

/* The following functions are meant to test the correctness of the msg_queue.
 *
 */
void PipeQueue::CallE()
{
    // for(int i=0; i<20; i++) {
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
    // int i=0;
    // while(i < 20) {
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
