#include "leader_leader.h"
#include "proto_utils.h"

// TODO:
// - Make replication factor a variable in the config file
// - Make sure you're not leaking memory
// - Make sure the counter isn't too inefficient
void runLeaderReceiveThread(
    const std::shared_ptr<Pipeline> pipeline, const std::shared_ptr<Acknowledgment> acknowledgment,
    const std::shared_ptr<iothread::MessageQueue<acknowledgment_tracker::ResendData>> resendDataQueue,
    const std::shared_ptr<QuorumAcknowledgment> quorumAck, const NodeConfiguration configuration,
    const std::shared_ptr<iothread::MessageQueue<scrooge::CrossChainMessage>> receivedMessageQueue)
{
    // SPDLOG_CRITICAL("RECV THREAD TID {}", gettid());
    uint64_t timedMessages{};
    pipeline::ReceivedCrossChainMessage receivedMessage{};
    scrooge::CrossChainMessage crossChainMessage;

    while (not is_test_over())
    {
        if (receivedMessage.message == nullptr)
        {
            receivedMessage = pipeline->RecvFromOtherRsm();

            // 1. If node receives messages from other RSM, process & rebroadcast
            if (receivedMessage.message != nullptr)
            {
                const auto [message, senderId] = receivedMessage;
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
                    timedMessages += is_test_recording();
                }
#if WRITE_DR || WRITE_CCF
        while (not receivedMessageQueue->try_enqueue(std::move(crossChainMessage)) && not is_test_over());
#endif
            }
        }

        if (receivedMessage.message != nullptr)
        {
            // Rebroadcasts the message to the RSM
            bool success = pipeline->rebroadcastToOwnRsm(receivedMessage.message);
            if (success)
            {
                receivedMessage.message = nullptr;
            }
        }

        const auto [broadcast_msg, broadcast_senderId] = pipeline->RecvFromOwnRsm();
        // 2. Process requests received from own RSM
        if (broadcast_msg)
        {
            const auto messageData = nng_msg_body(broadcast_msg);
            const auto messageSize = nng_msg_len(broadcast_msg);
            // SPDLOG_CRITICAL("Leader Message size: {}", messageSize);
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
        }
    }

    if (receivedMessage.message)
    {
        nng_msg_free(receivedMessage.message);
    }
    addMetric("local_messages_received", 0);
    addMetric("foreign_messages_received", timedMessages);
    addMetric("max_acknowledgment", acknowledgment->getAckIterator().value_or(0));
    addMetric("max_quorum_acknowledgment", quorumAck->getCurrentQuack().value_or(0));
}

template <bool kIsUsingFile>
static void runLeaderSendThread(
    const std::shared_ptr<iothread::MessageQueue<scrooge::CrossChainMessageData>> messageInput,
    const std::shared_ptr<Pipeline> pipeline, const std::shared_ptr<Acknowledgment> acknowledgment,
    const std::shared_ptr<iothread::MessageQueue<acknowledgment_tracker::ResendData>> resendDataQueue,
    const std::shared_ptr<QuorumAcknowledgment> quorumAck, const NodeConfiguration configuration)
{
    bindThreadToCpu(1);
    SPDLOG_CRITICAL("Leader Send Thread starting with TID = {}", gettid());

    uint64_t numMessagesSent{};
    Acknowledgment sentMessages{};
    if (configuration.kNodeId != leader_id)
    {
        addMetric("transfer_strategy", "Leader");
        addMetric("num_msgs_sent", numMessagesSent);
        SPDLOG_CRITICAL("NOT DESIGNATED SENDER, NO MESSAGES SENT. SENDING THREAD EXITING");
        return;
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
            while (messageInput->try_dequeue(newMessageData) && not is_test_over())
                std::this_thread::sleep_for(.1ms);
        }
        const auto curSequenceNumber = newMessageData.sequence_number();
        auto curTime = std::chrono::steady_clock::now();
        // SPDLOG_CRITICAL("SEND: Created new data and sequence number!");
        if constexpr (kIsUsingFile)
        {
            // SPDLOG_CRITICAL("SEND: Created new data and sequence number with size {}!",
            // newMessageData.message_content().size());
            pipeline->SendFileToOtherRsm(configuration.kNodeId % configuration.kOtherNetworkSize,
                                            std::move(newMessageData), nullptr, curTime);
        }
        else
        {
            // SPDLOG_CRITICAL("SENDING TO WRONG PLACE");
            pipeline->SendToOtherRsm(configuration.kNodeId % configuration.kOtherNetworkSize,
                                        std::move(newMessageData), nullptr, curTime);
        }
        sentMessages.addToAckList(curSequenceNumber);
        // quorumAck->updateNodeAck(0, 0ULL - 1, sentMessages.getAckIterator().value_or(0));
        numMessagesSent++;
        // SPDLOG_CRITICAL("SEND: Done with this iteration! Quack is at: {}",
        // sentMessages.getAckIterator().value_or(0));
    }

    addMetric("transfer_strategy", "Leader");
    addMetric("num_msgs_sent", numMessagesSent);
    SPDLOG_INFO("ALL CROSS CONSENSUS PACKETS SENT : send thread exiting");
}

void runFileLeaderSendThread(
    std::shared_ptr<iothread::MessageQueue<scrooge::CrossChainMessageData>> messageInput,
    std::shared_ptr<Pipeline> pipeline, std::shared_ptr<Acknowledgment> acknowledgment,
    std::shared_ptr<iothread::MessageQueue<acknowledgment_tracker::ResendData>> resendDataQueue,
    std::shared_ptr<QuorumAcknowledgment> quorumAck, NodeConfiguration configuration)
{
    constexpr bool kIsUsingFile = true;
    runLeaderSendThread<kIsUsingFile>(messageInput, pipeline, acknowledgment, resendDataQueue, quorumAck,
                                      configuration);
}

void runLeaderSendThread(std::shared_ptr<iothread::MessageQueue<scrooge::CrossChainMessageData>> messageInput,
                         std::shared_ptr<Pipeline> pipeline, std::shared_ptr<Acknowledgment> acknowledgment,
                         std::shared_ptr<iothread::MessageQueue<acknowledgment_tracker::ResendData>> resendDataQueue,
                         std::shared_ptr<QuorumAcknowledgment> quorumAck, NodeConfiguration configuration)
{
    constexpr bool kIsUsingFile = false;
    runLeaderSendThread<kIsUsingFile>(messageInput, pipeline, acknowledgment, resendDataQueue, quorumAck,
                                      configuration);
}
