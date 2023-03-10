#pragma once

#include <atomic>
#include <map>
#include <mutex>
#include <unordered_map>

#include "global.h"

// This class counts messages which a sufficient amount (kQuorumSize) of nodes have reportedly accepted
// This is done because if there are kQuorumSize-1 malicious nodes, we can accept messages that kQuorumSize other nodes
// have accepted
class QuorumAcknowledgment
{
  public:
    QuorumAcknowledgment(uint64_t quorumSize);
    void updateNodeAck(uint64_t nodeId, uint64_t ackValue);
    std::optional<uint64_t> getNodeAck(uint64_t nodeId) const;
    std::optional<uint64_t> getCurrentQuack() const;

  private:
    uint64_t getNodesAtAck(uint64_t ack) const;
    mutable std::recursive_mutex mMutex;
    // Number of nodes to accept a message
    const uint64_t kQuorumSize{};
    // mNodeToAck[node] = node.ackCount
    // node.ackCount = last consecutive value received by the node
    std::unordered_map<uint64_t, uint64_t> mNodeToAck;
    // mAckToNodeCount[Ack_Count] = |{node | mNodeToAck[node] == AckCount}|
    std::map<uint64_t, uint64_t> mAckToNodeCount;
    // mQuorumAck
    //     = argmax_i {kQuorumSize >= mAckToNodeCount[i] + mAckToNodeCount[i+1] + ... + mAckToNodeCount[inf]}
    // This represents the current acknowledged value of the entire quorum
    std::atomic<std::optional<uint64_t>> mQuorumAck{std::nullopt};

    uint64_t mNumNodesInCurQuorum = 0;
};
