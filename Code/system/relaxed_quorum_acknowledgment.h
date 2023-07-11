#pragma once

#include <map>
#include <unordered_map>

#include "global.h"

// This class counts messages which a sufficient amount (kQuorumStakeSize) of stake has reportedly accepted
// This is done because if there are kQuorumStakeSize-1 stake in malicious hands, we can accept messages that
// kQuorumStakeSize much accepts WARNING THIS CLASS ASSUMES STAKE NEVER CHANGES
class RelaxedQuorumAcknowledgment
{
  public:
    RelaxedQuorumAcknowledgment(uint64_t quorumStakeSize);
    // returns current quorum ack after update
    std::optional<uint64_t> updateNodeAck(uint64_t nodeId, const uint64_t nodeStake, uint64_t ackValue);
    void reset();
    std::optional<uint64_t> getNodeAck(uint64_t nodeId) const;
    std::optional<uint64_t> getCurrentQuack() const;

  private:
    uint64_t getStakeAtAck(uint64_t ack) const;
    // Total stake needed to accept a message
    const uint64_t kQuorumStakeSize{};
    // mNodeToAck[node] = node.ackCount
    // node.ackCount = highest cummulative ack received by the node
    std::unordered_map<uint64_t, uint64_t> mNodeToAck;
    // mAckToStakeCount[AckCount] = \Sum{node.stake | mNodeToAck[node] == AckCount}
    std::map<uint64_t, uint64_t> mAckToStakeCount;
    // mQuorumAck
    //     = argmax_i {kQuorumSize >= mAckToNodeCount[i] + mAckToNodeCount[i+1] + ... + mAckToNodeCount[inf]}
    // This represents the current acknowledged value of the entire quorum
    std::optional<uint32_t> mQuorumAck{std::nullopt}; // only uint32_t for easy lockfree atomic optional

    uint64_t mStakeInCurQuorum = 0;
};
