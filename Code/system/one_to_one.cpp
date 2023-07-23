#include "iothread.h"

#include "proto_utils.h"

void runOneToOneSendThread(
    const std::shared_ptr<iothread::MessageQueue<scrooge::CrossChainMessageData>> messageInput,
    const std::shared_ptr<Pipeline> pipeline, const std::shared_ptr<Acknowledgment> acknowledgment,
    const std::shared_ptr<iothread::MessageQueue<acknowledgment_tracker::ResendData>> resendDataQueue,
    const std::shared_ptr<QuorumAcknowledgment> quorumAck, const NodeConfiguration configuration)
{
    bindThreadToCpu(2);
    SPDLOG_CRITICAL("Send Thread starting with TID = {}", gettid());
    const auto &[kOwnNetworkSize, kOtherNetworkSize, kOwnNetworkStakes, kOtherNetworkStakes, kOwnMaxNumFailedStake,
                 kOtherMaxNumFailedStake, kNodeId, kLogPath, kWorkingDir] = configuration;

    uint64_t numMessagesSent{};
    Acknowledgment sentMessages{};
    while (not is_test_over())
    {
        scrooge::CrossChainMessageData newMessageData = util::getNextMessage();
        const auto curSequenceNumber = newMessageData.sequence_number();
        auto curTime = std::chrono::steady_clock::now();

        pipeline->SendToOtherRsm(kNodeId % kOtherNetworkSize, std::move(newMessageData), nullptr, curTime);
        sentMessages.addToAckList(curSequenceNumber);
        quorumAck->updateNodeAck(0, 0ULL - 1, sentMessages.getAckIterator().value_or(0));
        numMessagesSent++;
    }

    addMetric("transfer_strategy", "One-to-One");
    addMetric("num_msgs_sent", numMessagesSent);
    SPDLOG_INFO("ALL CROSS CONSENSUS PACKETS SENT : send thread exiting");
}

void runUnfairOneToOneSendThread(
    const std::shared_ptr<iothread::MessageQueue<scrooge::CrossChainMessageData>> messageInput,
    const std::shared_ptr<Pipeline> pipeline, const std::shared_ptr<Acknowledgment> acknowledgment,
    const std::shared_ptr<std::vector<std::unique_ptr<AcknowledgmentTracker>>> ackTrackers,
    const std::shared_ptr<QuorumAcknowledgment> quorumAck, const NodeConfiguration configuration)
{
    bindThreadToCpu(2);
    SPDLOG_CRITICAL("Unfair One to One Send Thread starting with TID = {}", gettid());
    const auto &[kOwnNetworkSize, kOtherNetworkSize, kOwnNetworkStakes, kOtherNetworkStakes, kOwnMaxNumFailedStake,
                 kOtherMaxNumFailedStake, kNodeId, kLogPath, kWorkingDir] = configuration;

    uint64_t numMessagesSent{};
    Acknowledgment sentMessages{};
    while (not is_test_over())
    {
        scrooge::CrossChainMessageData newMessageData = util::getNextMessage();
        const auto curSequenceNumber = newMessageData.sequence_number();
        // SPDLOG_CRITICAL("Sequence number in unfair: {} ", curSequenceNumber);
        auto curTime = std::chrono::steady_clock::now();
        if (curSequenceNumber % kOtherNetworkSize != kNodeId)
        {
            continue;
        }
        pipeline->SendToOtherRsm(kNodeId % kOtherNetworkSize, std::move(newMessageData), nullptr, curTime);
        // sentMessages.addToAckList(curSequenceNumber);
        quorumAck->updateNodeAck(0, 0ULL - 1, curSequenceNumber);
        numMessagesSent++;
    }

    addMetric("transfer_strategy", "One-to-One");
    addMetric("num_msgs_sent", numMessagesSent);
    SPDLOG_INFO("ALL CROSS CONSENSUS PACKETS SENT : send thread exiting");
}