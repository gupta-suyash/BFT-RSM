#include "cross_chain_library.h"

bool setup_cross_chain_library()
{
    // Setting up the queue
    unique_ptr<PipeQueue> sp_queue = make_unique<PipeQueue>();
    sp_qptr = sp_queue.get();
    // sp_qptr->Init();

    // Creating and starting Sender IOThreads.
    unique_ptr<SendThread> snd_obj = make_unique<SendThread>();
    snd_obj->Init(0);

    // Creating and starting Receiver IOThreads.
    unique_ptr<RecvThread> rcv_obj = make_unique<RecvThread>();
    rcv_obj->Init(1);

    snd_obj->thd_.join();
    rcv_obj->thd_.join();

    return true;
}

bool send_cross_chain_transaction()
{
    Acknowledgment *ack_obj = new Acknowledgment();
    QuorumAcknowledgment *quack_obj = new QuorumAcknowledgment();

    return true;
    // MORE!
}
