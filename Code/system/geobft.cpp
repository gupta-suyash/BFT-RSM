#include "geobft.h"
#include "proto_utils.h"

// TODO:
// - Make replication factor a variable in the config file
// - Make sure you're not leaking memory
// - Make sure the counter isn't too inefficient
void runGeoBFTReceiveThread(
    const std::shared_ptr<Pipeline> pipeline, const std::shared_ptr<Acknowledgment> acknowledgment,
    const std::shared_ptr<iothread::MessageQueue<acknowledgment_tracker::ResendData>> resendDataQueue,
    const std::shared_ptr<QuorumAcknowledgment> quorumAck, const NodeConfiguration configuration,
    const std::shared_ptr<iothread::MessageQueue<scrooge::CrossChainMessage>> receivedMessageQueue)
{
    SPDLOG_CRITICAL("GEOBFT RECEIVE THREAD TID {}", gettid());
    uint64_t timedMessages{};
    scrooge::CrossChainMessage crossChainMessage;
    uint64_t localMsgsReceived{};
    pipeline::ReceivedCrossChainMessage receivedMessage{};
    uint64_t numrb{};

    while (not is_test_over())
    {
        // 1. If node has nothing to re-broadcast, try to receive from other RSM
        if (receivedMessage.message == nullptr)
        {
            receivedMessage = pipeline->RecvFromOtherRsm();
        }

        // 2. If node has a message to re-broadcast, try to rebroadcast
        if (receivedMessage.message != nullptr)
        {
            const auto isRebroadcastSuccessful = pipeline->rebroadcastToOwnRsm(receivedMessage.message);
            if (isRebroadcastSuccessful)
            {
                // forget message now that its rebroadcast is successful
                receivedMessage.message = nullptr;
                numrb++;
            }
        }

        // 2. Process requests received from own RSM
        const auto [broadcast_msg, broadcast_senderId] = pipeline->RecvFromOwnRsm();
        if (broadcast_msg)
        {
            localMsgsReceived++;
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
            }
            
            quorumAck->updateNodeAck(0, 0ULL - 1, acknowledgment->getAckIterator().value_or(0));
#if WRITE_DR || WRITE_CCF
        while (not receivedMessageQueue->try_enqueue(std::move(crossChainMessage)) && not is_test_over());
#endif
        }
    }
    
    if (receivedMessage.message)
    {
        nng_msg_free(receivedMessage.message);
    }

    addMetric("local_messages_received", localMsgsReceived);
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

    constexpr auto sender_id = 0;

   
    if (configuration.kNodeId != sender_id)
    {
        if constexpr (!kIsUsingFile)
        {
            // dequeue all messages to free up the application
            scrooge::CrossChainMessageData newMessageData;
            while (not is_test_over())
            {
                while (! messageInput->try_dequeue(newMessageData) && not is_test_over());
            }
        }
        else
        {
            // not the sender and no messages are real because we're running file
            return;
        }
    }

    while (not is_test_over())
    {
        scrooge::CrossChainMessageData newMessageData;
        if constexpr (kIsUsingFile)
        {
            newMessageData = util::getNextMessage();
        }
        else
        {
            while (! messageInput->try_dequeue(newMessageData) && not is_test_over());
        }
        const auto curSequenceNumber = newMessageData.sequence_number();
        auto curTime = std::chrono::steady_clock::now();
        // SPDLOG_CRITICAL("SEND: Created new data and sequence number!");
        if constexpr (kIsUsingFile)
        {
            pipeline->SendFileToGeoBFTQuorumOtherRsm(std::move(newMessageData), curTime);
        }
        else
        {
            pipeline->SendToGeoBFTQuorumOtherRsm(std::move(newMessageData), curTime);
        }
        sentMessages.addToAckList(curSequenceNumber);
        numMessagesSent++;
    }

    addMetric("send_msg_block", sentMessages.getAckIterator().value_or(0));
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
