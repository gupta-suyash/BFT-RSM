#include "geobft.h"
#include "proto_utils.h"

// TODO for sending:
// Send to only f+1 other nodes as opposed to all
void runGeoBFTReceiveThread(
    const std::shared_ptr<Pipeline> pipeline, const std::shared_ptr<Acknowledgment> acknowledgment,
    const std::shared_ptr<iothread::MessageQueue<acknowledgment_tracker::ResendData>> resendDataQueue,
    const std::shared_ptr<QuorumAcknowledgment> quorumAck, const NodeConfiguration configuration)
{
    SPDLOG_CRITICAL("RECV THREAD TID {}", gettid());
    uint64_t timedMessages{};
    scrooge::CrossChainMessage crossChainMessage;

    while (not is_test_over())
    {
        const auto [message, senderId] = pipeline->RecvFromOtherRsm();
        if (not message)
        {
            std::this_thread::yield();
            continue;
        }

        const auto messageData = nng_msg_body(message);
        const auto messageSize = nng_msg_len(message);
        bool success = crossChainMessage.ParseFromArray(messageData, messageSize);
        if (not success)
        {
            SPDLOG_CRITICAL("Cannot parse foreign message");
        }
 
        for (const auto &messageData : crossChainMessage.data())
        {
            
            acknowledgment->addToAckList(messageData.sequence_number());
            timedMessages += is_test_recording();
        }
        
        // Rebroadcasts the message to the RSM
        if (message)
        {
            bool success = pipeline->rebroadcastToOwnRsm(message);
            if (success)
            {
                message = nullptr; 
            }
        }
            const auto [message, senderId] = pipeline->RecvFromOwnRsm();
            if (message)
            {
                const auto messageData = nng_msg_body(message);
                const auto messageSize = nng_msg_len(message);
                bool success = crossChainMessage.ParseFromArray(messageData, messageSize);
                nng_msg_free(message);
                if (not success)
                {
                    SPDLOG_CRITICAL("Cannot parse local message");
                }

                for (const auto &messageData : crossChainMessage.data())
                {
                    acknowledgment->addToAckList(messageData.sequence_number());
                    timedMessages += is_test_recording();
                }
            }
            nng_msg_free(message);
        }
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
    while (not is_test_over())
    {

        while (not is_test_over())
        {
            scrooge::CrossChainMessageData newMessageData = util::getNextMessage();
            const auto curSequenceNumber = newMessageData.sequence_number();
            auto curTime = std::chrono::steady_clock::now();

            if constexpr (kIsUsingFile)
            {
                pipeline->SendFileToGeoBFTQuorumOtherRsm(std::move(newMessageData), curTime);
            }
            else
            {
                pipeline->SendToGeoBFTQuorumOtherRsm(std::move(newMessageData), curTime);
            }
            sentMessages.addToAckList(curSequenceNumber);
            quorumAck->updateNodeAck(0, 0ULL - 1, sentMessages.getAckIterator().value_or(0));
            numMessagesSent++;
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
