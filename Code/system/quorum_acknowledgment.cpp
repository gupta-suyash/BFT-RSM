#include "quorum_acknowledgment.h"

#include <assert.h>

QuorumAcknowledgment::QuorumAcknowledgment(const uint64_t quorumSize) : kQuorumSize(quorumSize)
{
}

/* Updates or Adds the current Ack value of a node. non-monotonic increasing updates are ignored.
 *
 * @param nodeId is the value of the node to be updated/added.
 * @param ackValue is the ack value of the node.
 */
void QuorumAcknowledgment::updateNodeAck(const uint64_t nodeId, const uint64_t ackValue)
{
    const auto curNodeEntry = mNodeToAck.find(nodeId);
    const auto curQuorumAck = mQuorumAck.load(std::memory_order_relaxed);

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
        const auto acksLeftAtNode = --mAckToNodeCount[oldAckValue];

        if (acksLeftAtNode == 0)
        {
            // Don't store empty entries
            mAckToNodeCount.erase(oldAckValue);
        }

        // Decrement the old number of acks in the quorum
        if (!curQuorumAck.has_value() || curQuorumAck <= oldAckValue)
        {
            mNumNodesInCurQuorum--;
        }
    }

    // Update data structures
    mNodeToAck[nodeId] = ackValue;
    mAckToNodeCount[ackValue]++;

    // Update mNumNodesInCurQuorum
    if (!curQuorumAck.has_value() || curQuorumAck <= ackValue)
    {
        mNumNodesInCurQuorum++;
    }

    const auto nodesAtCurQuack = (curQuorumAck.has_value()) ? getNodesAtAck(curQuorumAck.value()) : 0;
    const auto nodesAboveCurQuorum = mNumNodesInCurQuorum - nodesAtCurQuack;

    const auto isNewQuorum = nodesAboveCurQuorum >= kQuorumSize;
    if (isNewQuorum)
    {
        mNumNodesInCurQuorum -= nodesAtCurQuack;

        if (curQuorumAck.has_value())
        {
            const auto oldVal = *curQuorumAck;
            mQuorumAck.store(mAckToNodeCount.upper_bound(oldVal)->first, std::memory_order_release);
        }
        else
        {
            // Lowest ack is the new quorum ack
            mQuorumAck.store(std::cbegin(mAckToNodeCount)->first, std::memory_order_release);
        }
    }
}

uint64_t QuorumAcknowledgment::getNodesAtAck(uint64_t ack) const
{
    const auto ackToNodeCountIt = mAckToNodeCount.find(ack);
    if (std::cend(mAckToNodeCount) == ackToNodeCountIt)
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
