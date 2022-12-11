#pragma once

#include <chrono>
#include <mutex>
#include <optional>
#include <unordered_map>

#include "global.h"

namespace acknowledgment_tracker
{
// Stores information about the node's data
struct NodeAckData
{
    // The highest cummulative acknowledgment value received by this node
    // Should be monotone non-decreasing in the honest case TODO punish visibly byzantine nodes
    uint64_t acknowledgmentValue{};
    // The number of times the node has sent this highest value repeatedly
    uint64_t repeatNumber{};
    // The time when the node sent its first ack of acknowledgmentValue
    std::chrono::steady_clock::time_point initialAckTime{};
};
}; // namespace acknowledgment_tracker

// This class keeps track of the aggregate number of times each node has reported the same acknowledgment value in
// sequence Also keeps track of the aggregate minimum time when each node got stuck at each acknowledgment value This is
// what will be used to determine when a node should resend a message
class AcknowledgmentTracker
{
  public:
    void updateNodeData(uint64_t nodeId, uint64_t acknowledgmentValue, std::chrono::steady_clock::time_point curTime);
    std::optional<std::chrono::steady_clock::time_point> getAggregateInitialAckTime(uint64_t acknowledgmentValue) const;
    uint64_t getAggregateRepeatedAckCount(uint64_t acknowledgmentValue) const;

  private:
    void incrementAggregates(const acknowledgment_tracker::NodeAckData &newNodeData);
    void decrementAggregates(const acknowledgment_tracker::NodeAckData &newNodeData);

    mutable std::recursive_mutex mMutex;

    std::unordered_map<uint64_t, acknowledgment_tracker::NodeAckData> mNodeData;
    // mAggregateAckCount[ack] = \Sum mNodeData[node].repeatNumber | mNodeData[node].ack == ack
    std::unordered_map<uint64_t, uint64_t> mAggregateAckCount;
    // mAggregateInitialAckTime[ack] = min mNodeData[node].initialAckTime | mNodeData[node].acknowledgmentValue == ack
    std::unordered_map<uint64_t, std::chrono::steady_clock::time_point> mAggregateInitialAckTime;
};
