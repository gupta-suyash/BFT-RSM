#include "iothread.h"

#include "proto_utils.h"

template <bool kIsUsingFile>
static void runOneToOneSendThread(
    const std::shared_ptr<iothread::MessageQueue<scrooge::CrossChainMessageData>> messageInput,
    const std::shared_ptr<Pipeline> pipeline, const std::shared_ptr<Acknowledgment> acknowledgment,
    const std::shared_ptr<iothread::MessageQueue<acknowledgment_tracker::ResendData>> resendDataQueue,
    const std::shared_ptr<QuorumAcknowledgment> quorumAck, const NodeConfiguration configuration)
{
    bindThreadToCpu(1);
    SPDLOG_CRITICAL("Send Thread starting with TID = {}", gettid());
    const auto &[kOwnNetworkSize, kOtherNetworkSize, kOwnNetworkStakes, kOtherNetworkStakes, kOwnMaxNumFailedStake,
                 kOtherMaxNumFailedStake, kNodeId, kLogPath, kWorkingDir] = configuration;

    uint64_t numMessagesSent{};
    Acknowledgment sentMessages{};
    while (not is_test_over())
    {
        scrooge::CrossChainMessageData newMessageData{};
        if constexpr (kIsUsingFile)
        {
            newMessageData = util::getNextMessage();
        }
        else
        {
            while (not messageInput->try_dequeue(newMessageData) && not is_test_over())
                std::this_thread::sleep_for(.1ms);
        }

        const auto curSequenceNumber = newMessageData.sequence_number();
        auto curTime = std::chrono::steady_clock::now();

        if constexpr (kIsUsingFile)
        {
            pipeline->SendFileToOtherRsm(kNodeId % kOtherNetworkSize, std::move(newMessageData), nullptr, curTime);
        }
        else
        {
            pipeline->SendToOtherRsm(kNodeId % kOtherNetworkSize, std::move(newMessageData), nullptr, curTime);
        }

        sentMessages.addToAckList(curSequenceNumber);
        quorumAck->updateNodeAck(0, 0ULL - 1, sentMessages.getAckIterator().value_or(0));
        numMessagesSent++;
    }

    addMetric("transfer_strategy", "One-to-One");
    addMetric("num_msgs_sent", numMessagesSent);
    SPDLOG_INFO("ALL CROSS CONSENSUS PACKETS SENT : send thread exiting");
}

void runFileOneToOneSendThread(
    std::shared_ptr<iothread::MessageQueue<scrooge::CrossChainMessageData>> messageInput,
    std::shared_ptr<Pipeline> pipeline, std::shared_ptr<Acknowledgment> acknowledgment,
    std::shared_ptr<iothread::MessageQueue<acknowledgment_tracker::ResendData>> resendDataQueue,
    std::shared_ptr<QuorumAcknowledgment> quorumAck, NodeConfiguration configuration)
{
    constexpr bool kIsUsingFile = true;
    runOneToOneSendThread<kIsUsingFile>(messageInput, pipeline, acknowledgment, resendDataQueue, quorumAck,
                                        configuration);
}

void runOneToOneSendThread(std::shared_ptr<iothread::MessageQueue<scrooge::CrossChainMessageData>> messageInput,
                           std::shared_ptr<Pipeline> pipeline, std::shared_ptr<Acknowledgment> acknowledgment,
                           std::shared_ptr<iothread::MessageQueue<acknowledgment_tracker::ResendData>> resendDataQueue,
                           std::shared_ptr<QuorumAcknowledgment> quorumAck, NodeConfiguration configuration)
{
    constexpr bool kIsUsingFile = false;
    runOneToOneSendThread<kIsUsingFile>(messageInput, pipeline, acknowledgment, resendDataQueue, quorumAck,
                                        configuration);
}

template <bool kIsUsingFile>
static void runUnfairOneToOneSendThread(
    const std::shared_ptr<iothread::MessageQueue<scrooge::CrossChainMessageData>> messageInput,
    const std::shared_ptr<Pipeline> pipeline, const std::shared_ptr<Acknowledgment> acknowledgment,
    const std::shared_ptr<iothread::MessageQueue<acknowledgment_tracker::ResendData>> resendDataQueue,
    const std::shared_ptr<QuorumAcknowledgment> quorumAck, const NodeConfiguration configuration)
{
    bindThreadToCpu(1);
    SPDLOG_CRITICAL("Unfair One to One Send Thread starting with TID = {}", gettid());
    const auto &[kOwnNetworkSize, kOtherNetworkSize, kOwnNetworkStakes, kOtherNetworkStakes, kOwnMaxNumFailedStake,
                 kOtherMaxNumFailedStake, kNodeId, kLogPath, kWorkingDir] = configuration;

    uint64_t numMessagesSent{};
    Acknowledgment sentMessages{};
    while (not is_test_over())
    {
        scrooge::CrossChainMessageData newMessageData{};
        if constexpr (kIsUsingFile)
        {
            newMessageData = util::getNextMessage();
        }
        else
        {
            while (not messageInput->try_dequeue(newMessageData) && not is_test_over())
                std::this_thread::sleep_for(.1ms);
        }
        const auto curSequenceNumber = newMessageData.sequence_number();
        // SPDLOG_CRITICAL("Sequence number in unfair: {} ", curSequenceNumber);
        auto curTime = std::chrono::steady_clock::now();
        if (curSequenceNumber % kOtherNetworkSize != kNodeId)
        {
            continue;
        }
        if constexpr (kIsUsingFile)
        {
            pipeline->SendFileToOtherRsm(kNodeId % kOtherNetworkSize, std::move(newMessageData), nullptr, curTime);
        }
        else
        {
            pipeline->SendToOtherRsm(kNodeId % kOtherNetworkSize, std::move(newMessageData), nullptr, curTime);
        }

        // sentMessages.addToAckList(curSequenceNumber);
        quorumAck->updateNodeAck(0, 0ULL - 1, curSequenceNumber);
        numMessagesSent++;
    }

    addMetric("transfer_strategy", "One-to-One");
    addMetric("num_msgs_sent", numMessagesSent);
    SPDLOG_INFO("ALL CROSS CONSENSUS PACKETS SENT : send thread exiting");
}

void runFileUnfairOneToOneSendThread(
    std::shared_ptr<iothread::MessageQueue<scrooge::CrossChainMessageData>> messageInput,
    std::shared_ptr<Pipeline> pipeline, std::shared_ptr<Acknowledgment> acknowledgment,
    std::shared_ptr<iothread::MessageQueue<acknowledgment_tracker::ResendData>> resendDataQueue,
    std::shared_ptr<QuorumAcknowledgment> quorumAck, NodeConfiguration configuration)
{
    constexpr bool kIsUsingFile = true;
    runUnfairOneToOneSendThread<kIsUsingFile>(messageInput, pipeline, acknowledgment, resendDataQueue, quorumAck,
                                              configuration);
}

void runUnfairOneToOneSendThread(
    std::shared_ptr<iothread::MessageQueue<scrooge::CrossChainMessageData>> messageInput,
    std::shared_ptr<Pipeline> pipeline, std::shared_ptr<Acknowledgment> acknowledgment,
    std::shared_ptr<iothread::MessageQueue<acknowledgment_tracker::ResendData>> resendDataQueue,
    std::shared_ptr<QuorumAcknowledgment> quorumAck, NodeConfiguration configuration)
{
    constexpr bool kIsUsingFile = false;
    runUnfairOneToOneSendThread<kIsUsingFile>(messageInput, pipeline, acknowledgment, resendDataQueue, quorumAck,
                                              configuration);
}

void runOneToOneReceiveThread(
    const std::shared_ptr<Pipeline> pipeline, const std::shared_ptr<Acknowledgment> acknowledgment,
    const std::shared_ptr<iothread::MessageQueue<acknowledgment_tracker::ResendData>> resendDataQueue,
    const std::shared_ptr<QuorumAcknowledgment> quorumAck, const NodeConfiguration configuration,
    const std::shared_ptr<iothread::MessageQueue<scrooge::CrossChainMessage>> receivedMessageQueue)
{
    SPDLOG_CRITICAL("RECV THREAD TID {}", gettid());
    uint64_t timedMessages{};
    scrooge::CrossChainMessage crossChainMessage;

    while (not is_test_over())
    {
        const auto [message, senderId] = pipeline->RecvFromOtherRsm();
        if (not message)
        {
            std::this_thread::sleep_for(.1ms);
            continue;
        }

        const auto messageData = nng_msg_body(message);
        const auto messageSize = nng_msg_len(message);
        bool success = crossChainMessage.ParseFromArray(messageData, messageSize);
        nng_msg_free(message);
        if (not success)
        {
            SPDLOG_CRITICAL("Cannot parse foreign message");
        }

        for (const auto &messageData : crossChainMessage.data())
        {
            if (not util::isMessageDataValid(messageData))
            {
                continue;
            }
            acknowledgment->addToAckList(messageData.sequence_number());
            timedMessages += is_test_recording();
        }
#if WRITE_DR || WRITE_CCF
        while (not receivedMessageQueue->try_enqueue(std::move(crossChainMessage)) && not is_test_over())
            ;
#endif
    }

    addMetric("local_messages_received", 0);
    addMetric("foreign_messages_received", timedMessages);
    addMetric("max_acknowledgment", acknowledgment->getAckIterator().value_or(0));
    addMetric("max_quorum_acknowledgment", quorumAck->getCurrentQuack().value_or(0));
}

void runUnfairOneToOneReceiveThread(
    const std::shared_ptr<Pipeline> pipeline, const std::shared_ptr<Acknowledgment> acknowledgment,
    const std::shared_ptr<iothread::MessageQueue<acknowledgment_tracker::ResendData>> resendDataQueue,
    const std::shared_ptr<QuorumAcknowledgment> quorumAck, const NodeConfiguration configuration,
    const std::shared_ptr<iothread::MessageQueue<scrooge::CrossChainMessage>> receivedMessageQueue)
{
    runOneToOneReceiveThread(pipeline, acknowledgment, resendDataQueue, quorumAck, configuration, receivedMessageQueue);
}
