#include "geobft.h"
#include "proto_utils.h"

// TODO:
// - Make replication factor a variable in the config file
// - Make sure you're not leaking memory
// - Make sure the counter isn't too inefficient
void runGeoBFTReceiveThread(
    const std::shared_ptr<Pipeline> pipeline, const std::shared_ptr<Acknowledgment> acknowledgment,
    const std::shared_ptr<iothread::MessageQueue<acknowledgment_tracker::ResendData>> resendDataQueue,
    const std::shared_ptr<QuorumAcknowledgment> quorumAck, const NodeConfiguration configuration)
{
    //SPDLOG_CRITICAL("RECV THREAD TID {}", gettid());
    uint64_t timedMessages{};
    scrooge::CrossChainMessage crossChainMessage;

    while (not is_test_over())
    {
        //SPDLOG_CRITICAL("RECEIVE: Beginning of while loop");
        auto receivedMessage = pipeline->RecvFromOtherRsm();
        const auto [message, senderId] = receivedMessage;
        
        // 1. If node receives messages from other RSM, process & rebroadcast
        if (message && receivedMessage.message)
        {
            //SPDLOG_CRITICAL("RECEIVE: Received a message!");
            const auto messageData = nng_msg_body(message);
            const auto messageSize = nng_msg_len(message);
            bool success = crossChainMessage.ParseFromArray(messageData, messageSize);
            if (not success)
            {
                SPDLOG_CRITICAL("Cannot parse foreign message"); // TODO: Why is it ok to continue?
            }
 
            for (const auto &messageData : crossChainMessage.data())
            {
                acknowledgment->addToAckList(messageData.sequence_number());
                //timedMessages += is_test_recording();
            }
            //SPDLOG_CRITICAL("RECEIVE: Sorted through message data!");
            
            // Rebroadcasts the message to the RSM
            success = pipeline->rebroadcastToOwnRsm(receivedMessage.message);
            if (not success) 
            {
                //SPDLOG_CRITICAL("Cannot rebroadcast message!");
            }
            receivedMessage.message = nullptr;
            //nng_msg_free(message);
            //SPDLOG_CRITICAL("RECEIVE: Rebroadcast to the rest of the RSMs!"); // CORRECT Up to here
        }
        
        const auto [broadcast_msg, broadcast_senderId] = pipeline->RecvFromOwnRsm();
        // 2. Process requests received from own RSM
        if (broadcast_msg)
        {
            const auto messageData = nng_msg_body(broadcast_msg);
            const auto messageSize = nng_msg_len(broadcast_msg);
            bool success = crossChainMessage.ParseFromArray(messageData, messageSize);
            nng_msg_free(broadcast_msg);
            if (not success)
            {
                SPDLOG_CRITICAL("Cannot parse broadcast message");
            }

            for (const auto &messageData : crossChainMessage.data())
            {
                acknowledgment->addToAckList(messageData.sequence_number());
                timedMessages += is_test_recording();
                quorumAck->updateNodeAck(0, 0ULL - 1, messageData.sequence_number());
            }
            //nng_msg_free(broadcast_msg);
        }
        //SPDLOG_CRITICAL("RECEIVE: Processed broadcast message!");
    }

    addMetric("local_messages_received", 0);
    addMetric("foreign_messages_received", timedMessages);
    addMetric("max_acknowledgment", acknowledgment->getAckIterator().value_or(0));
    addMetric("max_quorum_acknowledgment", quorumAck->getCurrentQuack().value_or(0));
}

template <bool kIsUsingFile>
static void runGeoBFTSendThread(
    const std::shared_ptr<iothread::MessageQueue<scrooge::CrossChainMessageData>> messageInput,
    const std::shared_ptr<Pipeline> pipeline, const std::shared_ptr<Acknowledgment> acknowledgment,
    const std::shared_ptr<iothread::MessageQueue<acknowledgment_tracker::ResendData>> resendDataQueue,
    const std::shared_ptr<QuorumAcknowledgment> quorumAck, const NodeConfiguration configuration)
{
    bindThreadToCpu(1);
    SPDLOG_CRITICAL("GeoBFT Send Thread starting with TID = {}", gettid());

    uint64_t numMessagesSent{};
    Acknowledgment sentMessages{};
    if (configuration.kNodeId != sender_id) {
        addMetric("transfer_strategy", "GeoBFT");
        addMetric("num_msgs_sent", numMessagesSent);
        SPDLOG_CRITICAL("NOT DESIGNATED SENDER, NO MESSAGES SENT. SENDING THREAD EXITING");
        return;
    }
    while (not is_test_over())
    {
        while (not is_test_over())
        {
            scrooge::CrossChainMessageData newMessageData = util::getNextMessage();
            const auto curSequenceNumber = newMessageData.sequence_number();
            auto curTime = std::chrono::steady_clock::now();
            //SPDLOG_CRITICAL("SEND: Created new data and sequence number!");
            if constexpr (kIsUsingFile)
            {
                pipeline->SendFileToGeoBFTQuorumOtherRsm(std::move(newMessageData), curTime);
            }
            else
            {
                pipeline->SendToGeoBFTQuorumOtherRsm(std::move(newMessageData), curTime);
            }
            sentMessages.addToAckList(curSequenceNumber);
            //quorumAck->updateNodeAck(0, 0ULL - 1, sentMessages.getAckIterator().value_or(0));
            //quorumAck->updateNodeAck(0, 0ULL - 1, sentMessages.getAckIterator().value_or(0));
            numMessagesSent++;
            //SPDLOG_CRITICAL("SEND: Done with this iteration! Quack is at: {}", sentMessages.getAckIterator().value_or(0));
      }
    }

    addMetric("transfer_strategy", "GeoBFT");
    addMetric("num_msgs_sent", numMessagesSent);
    SPDLOG_INFO("ALL CROSS CONSENSUS PACKETS SENT : send thread exiting");
}

void runFileGeoBFTSendThread(
    std::shared_ptr<iothread::MessageQueue<scrooge::CrossChainMessageData>> messageInput,
    std::shared_ptr<Pipeline> pipeline, std::shared_ptr<Acknowledgment> acknowledgment,
    std::shared_ptr<iothread::MessageQueue<acknowledgment_tracker::ResendData>> resendDataQueue,
    std::shared_ptr<QuorumAcknowledgment> quorumAck, NodeConfiguration configuration)
{
    constexpr bool kIsUsingFile = true;
    runGeoBFTSendThread<kIsUsingFile>(messageInput, pipeline, acknowledgment, resendDataQueue, quorumAck,
                                        configuration);
}

void runGeoBFTSendThread(std::shared_ptr<iothread::MessageQueue<scrooge::CrossChainMessageData>> messageInput,
                           std::shared_ptr<Pipeline> pipeline, std::shared_ptr<Acknowledgment> acknowledgment,
                           std::shared_ptr<iothread::MessageQueue<acknowledgment_tracker::ResendData>> resendDataQueue,
                           std::shared_ptr<QuorumAcknowledgment> quorumAck, NodeConfiguration configuration)
{
    constexpr bool kIsUsingFile = false;
    runGeoBFTSendThread<kIsUsingFile>(messageInput, pipeline, acknowledgment, resendDataQueue, quorumAck,
                                        configuration);
}
