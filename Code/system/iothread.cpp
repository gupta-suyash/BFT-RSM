#include "iothread.h"
#include "acknowledgement.h"

void runSendThread(std::shared_ptr<PipeQueue> pipeQueue, std::shared_ptr<Pipeline> pipeline)
{
    uint64_t bid = 1;
    uint64_t number_of_packets = get_number_of_packets();
    uint64_t lastSent = get_node_rsm_id();
    while (true)
    {
        if (bid < get_number_of_packets())
        {

            // Send to one node in other rsm.
            uint16_t nid = lastSent;

            // TODO: Get data from ipc, current code sequence number is broken
            scrooge::CrossChainMessage msg;
            msg.mutable_data()->set_sequence_number(bid);
            msg.mutable_data()->set_message_content("block");
            msg.set_ack_count(0); // use ack.h for this

            pipeQueue->Enqueue(std::move(msg));

            bid++;

            // Use PipeQueue to tell if there is a message to resend also !!
            pipeline->SendToOtherRsm(nid, std::move(msg));
            nid = (nid + 1) % get_nodes_rsm();
            lastSent = nid;
        }
    }
}

void runReceiveThread(std::shared_ptr<Pipeline> pipeline)
{
    std::optional<uint64_t> lastAckCount;
    while (true)
    {
        const auto newMessages = pipeline->RecvFromOtherRsm();
        pipeline->RecvFromOwnRsm();

        // Broadcast to all in own rsm.
        pipeline->SendToOwnRsm();

        // Updating the ack list for msg received.
        ack_obj->addToAckList(msg->data().sequence_number());

        // Updating the ack list for message received.
        ack_obj->addToAckList(message->data().sequence_number());

        for (size_t i = 0; i < msgs.size(); i++)
        {
            SendToOtherRsm(1, msgs.at(i));
        }

        const auto newAckCount = ack_obj->getAckIterator();
        if (lastAckCount != newAckCount)
        {
            SPDLOG_INFO("Node Ack Count Now at {}", newAckCount.value_or(0));
            lastAckCount = newAckCount;
        }
    }
}
