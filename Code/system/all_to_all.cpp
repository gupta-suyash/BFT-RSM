#include "all_to_all.h"

#include "proto_utils.h"

void runAllToAllReceiveThread(
    const std::shared_ptr<Pipeline> pipeline, const std::shared_ptr<Acknowledgment> acknowledgment,
    const std::shared_ptr<iothread::MessageQueue<acknowledgment_tracker::ResendData>> resendDataQueue,
    const std::shared_ptr<QuorumAcknowledgment> quorumAck, const NodeConfiguration configuration)
{
    SPDLOG_CRITICAL("RECV THREAD TID {}", gettid());
    uint64_t timedMessages{};

    while (not is_test_over())
    {
        const auto [message, senderId] = pipeline->RecvFromOtherRsm();
        if (not message)
        {
            std::this_thread::yield();
            continue;
        }

        for (const auto &messageData : message->data())
        {
            if (not util::isMessageDataValid(messageData))
            {
                continue;
            }
            acknowledgment->addToAckList(messageData.sequence_number());
            timedMessages += is_test_recording();
        }
    }

    addMetric("local_messages_received", 0);
    addMetric("foreign_messages_received", timedMessages);
    addMetric("max_acknowledgment", acknowledgment->getAckIterator().value_or(0));
    addMetric("max_quorum_acknowledgment", quorumAck->getCurrentQuack().value_or(0));
}

template <bool kIsUsingFile>
static void runAllToAllSendThread(
    const std::shared_ptr<iothread::MessageQueue<scrooge::CrossChainMessageData>> messageInput,
    const std::shared_ptr<Pipeline> pipeline, const std::shared_ptr<Acknowledgment> acknowledgment,
    const std::shared_ptr<iothread::MessageQueue<acknowledgment_tracker::ResendData>> resendDataQueue,
    const std::shared_ptr<QuorumAcknowledgment> quorumAck, const NodeConfiguration configuration)
{
    bindThreadToCpu(1);
    SPDLOG_CRITICAL("Send Thread starting with TID = {}", gettid());

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
                pipeline->SendFileToAllOtherRsm(std::move(newMessageData), curTime);
            }
            else
            {
                pipeline->SendToAllOtherRsm(std::move(newMessageData), curTime);
            }
            sentMessages.addToAckList(curSequenceNumber);
            quorumAck->updateNodeAck(0, 0ULL - 1, sentMessages.getAckIterator().value_or(0));
            numMessagesSent++;
        }
    }

    addMetric("transfer_strategy", "All-to-All");
    addMetric("num_msgs_sent", numMessagesSent);
    SPDLOG_INFO("ALL CROSS CONSENSUS PACKETS SENT : send thread exiting");
}

void runFileAllToAllSendThread(
    std::shared_ptr<iothread::MessageQueue<scrooge::CrossChainMessageData>> messageInput,
    std::shared_ptr<Pipeline> pipeline, std::shared_ptr<Acknowledgment> acknowledgment,
    std::shared_ptr<iothread::MessageQueue<acknowledgment_tracker::ResendData>> resendDataQueue,
    std::shared_ptr<QuorumAcknowledgment> quorumAck, NodeConfiguration configuration)
{
    constexpr bool kIsUsingFile = true;
    runAllToAllSendThread<kIsUsingFile>(messageInput, pipeline, acknowledgment, resendDataQueue, quorumAck,
                                        configuration);
}

void runAllToAllSendThread(std::shared_ptr<iothread::MessageQueue<scrooge::CrossChainMessageData>> messageInput,
                           std::shared_ptr<Pipeline> pipeline, std::shared_ptr<Acknowledgment> acknowledgment,
                           std::shared_ptr<iothread::MessageQueue<acknowledgment_tracker::ResendData>> resendDataQueue,
                           std::shared_ptr<QuorumAcknowledgment> quorumAck, NodeConfiguration configuration)
{
    constexpr bool kIsUsingFile = false;
    runAllToAllSendThread<kIsUsingFile>(messageInput, pipeline, acknowledgment, resendDataQueue, quorumAck,
                                        configuration);
}
