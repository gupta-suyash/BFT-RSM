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
    spdlog::debug("Current mQuorumAck = {}", mQuorumAck.value_or(0));

    const auto curNodeEntry = mNodeToAck.find(nodeId);

    const bool isUpdate = curNodeEntry != mNodeToAck.end();
    const bool isUpdateStale = isUpdate && ackValue <= curNodeEntry->second;
    if (isUpdateStale)
    {
        // The update would decrease or not change the node's current ack value
        return;
    }

    if (isUpdate)
    {
        // Decrement the old ack value
        const auto oldAckValue = curNodeEntry->second;
        mAckToNodeCount[oldAckValue]--;
    }

    // Update data structures
    mNodeToAck[nodeId] = ackValue;
    mAckToNodeCount[ackValue]++;

    // Update Current mQuorumAck -- can be done in log time with a pruned suffix tree
    uint64_t suffixSum = 0;
    for (auto it = std::rbegin(mAckToNodeCount); it != std::rend(mAckToNodeCount); it++)
    {
        const auto &[ackValue, nodesAtAck] = *it;
        suffixSum += nodesAtAck;

        if (suffixSum >= kQuorumSize)
        {
            mQuorumAck = ackValue;
            break;
        }
    }
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
