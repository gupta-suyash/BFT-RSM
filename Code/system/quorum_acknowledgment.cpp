#include "quorum_acknowledgment.h"

#include <assert.h>

QuorumAcknowledgment::QuorumAcknowledgment(const uint64_t quorumStakeSize) : kQuorumStakeSize(quorumStakeSize)
{
}

/* Updates or Adds the current Ack value of a node. non-monotonic increasing updates are ignored.
 *
 * @param nodeId is the value of the node to be updated/added.
 * @param ackValue is the ack value of the node.
 */
void QuorumAcknowledgment::updateNodeAck(const uint64_t nodeId, const uint64_t nodeStake, const uint64_t ackValue)
{
    const auto curNodeEntry = mNodeToAck.find(nodeId);
    auto curQuorumAck = mQuorumAck.load(std::memory_order_relaxed);

    const bool isUpdate = curNodeEntry != mNodeToAck.end();
    const bool isUpdateStale = isUpdate && (ackValue <= curNodeEntry->second);
    if (isUpdateStale)
    {
        // The update would decrease or not change the node's current ack value
        return;
    }

    if (isUpdate)
    {
        // Decrement the old ack value
        const auto oldAckValue = curNodeEntry->second;
        const auto stakeLeftAtNode = (mAckToStakeCount[oldAckValue] -= nodeStake);

        if (stakeLeftAtNode == 0)
        {
            // Don't store empty entries
            mAckToStakeCount.erase(oldAckValue);
        }

        // Decrement the stake in the quorum
        if (!curQuorumAck.has_value() || curQuorumAck <= oldAckValue)
        {
            mStakeInCurQuorum -= nodeStake;
        }
    }

    // Update data structures
    mNodeToAck[nodeId] = ackValue;
    mAckToStakeCount[ackValue] += nodeStake;

    // Update mStakeInCurQuorum
    if (!curQuorumAck.has_value() || curQuorumAck <= ackValue)
    {
        mStakeInCurQuorum += nodeStake;
    }

    const auto isFirstQuack = not curQuorumAck.has_value() && mStakeInCurQuorum >= kQuorumStakeSize;
    if (isFirstQuack)
    {
        curQuorumAck = mAckToStakeCount.begin()->first;
    }

    const auto isNoQuorumToUpdate = not curQuorumAck.has_value();
    if (isNoQuorumToUpdate)
    {
        return;
    }

    // This is linear in number of nodes -- could be made log time with a segment tree
    for (auto nextQuack = mAckToStakeCount.upper_bound(*curQuorumAck); nextQuack != mAckToStakeCount.end(); nextQuack++)
    {
        const auto stakeAtCurQuack = getStakeAtAck(curQuorumAck.value());
        const auto nodesAboveCurQuorum = mStakeInCurQuorum - stakeAtCurQuack;

        const auto isNewQuorum = nodesAboveCurQuorum >= kQuorumStakeSize;
        if (not isNewQuorum)
        {
            break;
        }

        mStakeInCurQuorum -= stakeAtCurQuack;
        curQuorumAck = nextQuack->first;
    }

    mQuorumAck.store(curQuorumAck, std::memory_order_relaxed);
}

uint64_t QuorumAcknowledgment::getStakeAtAck(uint64_t ack) const
{
    const auto ackToNodeCountIt = mAckToStakeCount.find(ack);
    if (std::cend(mAckToStakeCount) == ackToNodeCountIt)
    {
        return 0;
    }

    return ackToNodeCountIt->second;
}

std::optional<uint64_t> QuorumAcknowledgment::getNodeAck(const uint64_t nodeId) const
{
    assert(false && "reorganize code to be safe");
    const auto nodeEntry = mNodeToAck.find(nodeId);
    if (std::cend(mNodeToAck) == nodeEntry)
    {
        return std::nullopt;
    }
    return nodeEntry->second;
}

/* Get the value of variable quackValue or nullopt if it doesn't exist.
 *
 * @return mQuorumAck
 */
std::optional<uint64_t> QuorumAcknowledgment::getCurrentQuack() const
{
    return mQuorumAck.load(std::memory_order_acquire);
}
