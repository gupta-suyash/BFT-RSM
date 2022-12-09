#include "iothread.h"
#include "acknowledgement.h"
#include "pipeline.h"

#include <limits>

uint16_t SendThread::GetThreadId()
{
    return thd_id_;
}

void SendThread::Init(uint16_t thd_id)
{
    last_sent_ = get_node_rsm_id();
    thd_id_ = thd_id;
    thd_ = thread(&SendThread::Run, this);
}

void SendThread::Run()
{
    uint64_t bid = 1;
    uint64_t number_of_packets = 8000;
    while (true)
    {
        if (bid < number_of_packets)
        {

            // Send to one node in other rsm.
            uint16_t nid = GetLastSent();

            // TODO: Get data from ipc
            scrooge::CrossChainMessage msg;
            msg.mutable_data()->set_sequence_number(bid);
            msg.mutable_data()->set_message_content("block");
            msg.set_ack_count(0);

            in_queue.push(msg);

            bid++;

            bool did_send = pipe_ptr->SendToOtherRsm(nid, std::nullopt);
            // Did send to other RSM?
            if (did_send)
            {
                // Set the id of next node to send.
                nid = (nid + 1) % get_nodes_rsm();
                SetLastSent(nid);
            }
        }

        // Broadcast to all in own rsm.
        pipe_ptr->SendToOwnRsm();

        // Receiver thread code -- temporary.
        pipe_ptr->RecvFromOtherRsm();
        pipe_ptr->RecvFromOwnRsm();
    }
}

uint16_t SendThread::GetLastSent()
{
    return last_sent_;
}

void SendThread::SetLastSent(uint16_t id)
{
    last_sent_ = id;
}

uint16_t RecvThread::GetThreadId()
{
    return thd_id_;
}

void RecvThread::Init(uint16_t thd_id)
{
    thd_id_ = thd_id;
    thd_ = thread(&RecvThread::Run, this);
}

void RecvThread::Run()
{
    // cout << "RecvThread: " << GetThreadId() << endl;
    bool flag = true;
    while (true)
    {
        pipe_ptr->RecvFromOtherRsm();
        pipe_ptr->RecvFromOwnRsm();

        uint64_t bid = ack_obj->getAckIterator().value_or(0);
        if (bid < std::numeric_limits<uint64_t>::max() && flag)
        {
            cout << "Ack list at: " << bid << endl;
            if (bid == 499)
            {
                flag = false;
            }
        }
    }
}
