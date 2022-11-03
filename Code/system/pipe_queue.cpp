#include "pipe_queue.h"
#include <cstring>

/* Pushes a message to the queue.
 *
 * @param msg is the message to be queued..
 */
void PipeQueue::Enqueue(crosschain_proto::CrossChainMessage msg)
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
crosschain_proto::CrossChainMessage PipeQueue::Dequeue()
{
    crosschain_proto::CrossChainMessage msg;

    // Locking access to queue.
    msg_q_mutex.lock();

    // Popping the message; valid returns the status.
    // valid = msg_queue_.pop(msg);
    if (!msg_queue_.empty())
    {
        msg = msg_queue_.front();
        msg_queue_.pop();
        cout << "Dequeued: " << msg.sequence_id() << endl;
    }
    else
    {
        msg.set_sequence_id(0);
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
crosschain_proto::CrossChainMessage PipeQueue::EnqueueStore()
{
    crosschain_proto::CrossChainMessage msg;
    // Popping out the message from in_queue to send to other RSM.
    // valid = in_queue->pop(msg);

    if (!in_queue.empty())
    {
        msg = in_queue.front();
        in_queue.pop();
        if (msg.sequence_id() % get_nodes_rsm() != get_node_rsm_id())
        {
            // Any message that is not supposed to be sent by this node,
            // it pushes it to the store_queue.
            // cout << "Will store: " << msg.sequence_id() << " :: " << msg.transactions() << endl;

            store_queue_.push(msg);

            // TODO: Do we need this or this is extra memory alloc.
            msg.clear_sequence_id();
            msg.clear_ack_id();
            msg.clear_transactions();
            msg.set_sequence_id(0);
        }
    }
    else
    {
        // No message in the queue.
        msg.set_sequence_id(0);
    }

    return msg;
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
