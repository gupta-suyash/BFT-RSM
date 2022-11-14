#include "connect.h"
#include "data_comm.h"
#include "global.h"
#include <boost/lockfree/queue.hpp>
#include <queue>

void Init()
{
    // in_queue = new std::queue<scrooge::CrossChainMessage>(0);
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
void SendBlock(const uint64_t block_id, const char *block)
{
    // ProtoMessage *msg = ProtoMessage::SetMessage(block_id, block);
    scrooge::CrossChainMessage msg;
    msg.mutable_data()->set_sequence_number(block_id);
    msg.mutable_data()->set_message_content(block);
    msg.set_ack_count(0);

    // cout << "Message added: " << msg.data().sequence_number() << " :: " << msg.data().message_content() << endl;

    in_queue.push(msg);

    // TODO: Do we need to delete this msg?
}

void ReceiveBlock()
{
}
