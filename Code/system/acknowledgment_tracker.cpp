#include "acknowledgment_tracker.h"

AcknowledgmentTracker::AcknowledgmentTracker(uint64_t otherNetworkSize, uint64_t otherNetworkMaxFailedStake)
    : kOtherNetworkMaxFailedStake(otherNetworkMaxFailedStake), mNodeData(otherNetworkSize),
      staleAckQuorumCounter(otherNetworkMaxFailedStake + 1)
{
}

acknowledgment_tracker::ResendData AcknowledgmentTracker::update(uint64_t nodeId, uint64_t nodeStake,
                                                                 std::optional<uint64_t> acknowledgmentValue,
                                                                 std::optional<uint64_t> curQuackValue)
{
    const auto oldAcknowledgmentValue = mNodeData.at(nodeId).acknowledgmentValue;
    if (oldAcknowledgmentValue > curQuackValue)
    {
        // the node you're updating is unstuck
        return {};
    }
    if (mCurStuckQuorumAck == curQuackValue && mCurUnstuckStake > kOtherNetworkMaxFailedStake)
    {
        // this message is already delivered
        return {};
    }
    // TODO: Make this robust (should equal <= f1 + f2 + 1)
    if (mCurStuckQuorumAck == curQuackValue && mActiveResendData.isActive && mActiveResendData.resendNumber > 2)
    {
        return {};
    }

    updateNodeData(nodeId, acknowledgmentValue);
    updateAggregateData(nodeId, nodeStake, oldAcknowledgmentValue, acknowledgmentValue, curQuackValue);
    return updateActiveResendData();
}

void AcknowledgmentTracker::updateNodeData(uint64_t nodeId, std::optional<uint64_t> acknowledgmentValue)
{
    auto &curNodeData = mNodeData.at(nodeId);

    bool isNewAck = curNodeData.acknowledgmentValue < acknowledgmentValue;
    if (isNewAck)
    {
        curNodeData =
            acknowledgment_tracker::NodeAckData{.acknowledgmentValue = acknowledgmentValue, .repeatNumber = 0};
        return;
    }

    bool isStaleAck = curNodeData.acknowledgmentValue > acknowledgmentValue;
    if (isStaleAck)
    {
        return;
    }

    curNodeData.repeatNumber++;
}

void AcknowledgmentTracker::updateAggregateData(uint64_t nodeId, uint64_t nodeStake,
                                                std::optional<uint64_t> oldAcknowledgmentValue,
                                                std::optional<uint64_t> acknowledgmentValue,
                                                std::optional<uint64_t> curQuackValue)
{
    const bool isQuackUnstuck = mCurStuckQuorumAck != curQuackValue;
    if (isQuackUnstuck)
    {
        mCurStuckQuorumAck = curQuackValue;
        staleAckQuorumCounter.reset();
        mCurUnstuckStake = 0;
    }

    const bool isNodeJustUnstuck = oldAcknowledgmentValue <= curQuackValue && curQuackValue < acknowledgmentValue;
    if (isNodeJustUnstuck)
    {
        mCurUnstuckStake += nodeStake;
    }

    const bool isNodeAtCurQuorumAck = acknowledgmentValue == mCurStuckQuorumAck;
    if (isNodeAtCurQuorumAck)
    {
        const auto repeatAcks = mNodeData.at(nodeId).repeatNumber;
        // staleAckQuorumCounter counts quorums of repeated acks
        staleAckQuorumCounter.updateNodeAck(nodeId, nodeStake, repeatAcks);
    }
}

acknowledgment_tracker::ResendData AcknowledgmentTracker::updateActiveResendData()
{
    auto &curResendData = mActiveResendData;

    if (mCurUnstuckStake > kOtherNetworkMaxFailedStake)
    {
        if (curResendData.isActive)
        {
            mActiveResendData = {};
        }
        return {};
    }

    const auto sequenceNumberToResend = mCurStuckQuorumAck.value_or(0ULL - 1) + 1;
    const auto numRepeatedAckQuorums = staleAckQuorumCounter.getCurrentQuack();

    const bool isNoResendNeeded = numRepeatedAckQuorums < 1;
    if (isNoResendNeeded)
    {
        if (curResendData.isActive)
        {
            mActiveResendData = {};
        }
        return {};
    }

    // Small ints are so that reading/writing to the atomic doesn't use locks -- easy to remove
    const auto potentialNewResendData =
        acknowledgment_tracker::ResendData{.sequenceNumber = (uint32_t)sequenceNumberToResend,
                                           .resendNumber = (uint16_t)numRepeatedAckQuorums.value(),
                                           .isActive = true};

    const bool isCurResendDataOutdated = curResendData != potentialNewResendData;
    if (isCurResendDataOutdated)
    {
        mActiveResendData = potentialNewResendData;
        return mActiveResendData;
    }
    return {};
}

acknowledgment_tracker::ResendData AcknowledgmentTracker::getActiveResendData() const
{
    // TODO check if std::memory_order_acquire is too expensive (probably won't be with crypto)
    return mActiveResendData;
}
