#include "quorum_acknowledgement.h"

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
    // Lock class for thread safety
    std::scoped_lock lock{mMutex};

    const auto curNodeEntry = mNodeToAck.find(nodeId);

    const bool isUpdate = curNodeEntry != mNodeToAck.end();
    const bool isUpdateStale = isUpdate && (ackValue <= curNodeEntry->second);
    const bool isUpdateInvalid = ackValue == 0;
    if (isUpdateStale || isUpdateInvalid)
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
    }

    // Update data structures
    mNodeToAck[nodeId] = ackValue;
    mAckToNodeCount[ackValue]++;

    // Update mNumNodesInCurQuorum
    if (!mQuorumAck.has_value() || mQuorumAck < ackValue)
    {
        mNumNodesInCurQuorum++;
    }

    const auto nodesAtCurQuack = (mQuorumAck.has_value()) ? getNodesAtAck(mQuorumAck.value()) : 0;
    const auto nodesAboveCurQuorum = mNumNodesInCurQuorum - nodesAtCurQuack;

    const auto isNewQuorum = nodesAboveCurQuorum >= kQuorumSize;
    if (isNewQuorum)
    {
        mNumNodesInCurQuorum -= nodesAtCurQuack;

        if (mQuorumAck.has_value())
        {
            // Next lowest ack is the new quorum ack
            mQuorumAck = std::next(mAckToNodeCount.find(*mQuorumAck))->first;
        }
        else
        {
            // Lowest ack is the new quorum ack
            mQuorumAck = std::cbegin(mAckToNodeCount)->first;
        }
    }
}

uint64_t QuorumAcknowledgment::getNodesAtAck(uint64_t ack) const
{
    std::scoped_lock lock{mMutex};

    const auto ackToNodeCountIt = mAckToNodeCount.find(ack);
    if (std::cend(mAckToNodeCount) == ackToNodeCountIt)
    {
        return 0;
    }

    return ackToNodeCountIt->second;
}

std::optional<uint64_t> QuorumAcknowledgment::getNodeAck(const uint64_t nodeId) const
{
    std::scoped_lock lock{mMutex};
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
    std::scoped_lock lock{mMutex};
    return mQuorumAck;
}
