#pragma once

#include <atomic>
#include <chrono>
#include <optional>
#include <vector>

#include "global.h"
#include "relaxed_quorum_acknowledgment.h"

namespace acknowledgment_tracker
{
// Stores information about the node's data
struct NodeAckData
{
    std::optional<uint64_t> acknowledgmentValue{};
    // The number of times the node has sent the same acknowledgmentValue repeatedly
    uint64_t repeatNumber{0ULL - 1};
};
#pragma pack(push, 1)
struct ResendData
{
    uint32_t sequenceNumber{};
    uint16_t resendNumber{}; // counts from 1
    uint16_t isActive{};
    bool operator==(const ResendData &) const = default;
};
#pragma pack(pop)
static_assert(std::atomic<ResendData>{}.is_always_lock_free);
}; // namespace acknowledgment_tracker

// This class keeps track of the acknowledgments returned by each node to keep track of if/when nodes should retry
// sending messages what will be used to determine when a node should resend a message
class AcknowledgmentTracker
{
  public:
    AcknowledgmentTracker(uint64_t otherNetworkSize, uint64_t otherNetworkMaxFailedStake);

    acknowledgment_tracker::ResendData update(uint64_t nodeId, uint64_t nodeStake,
                                              std::optional<uint64_t> acknowledgmentValue,
                                              std::optional<uint64_t> curQuackValue);
    acknowledgment_tracker::ResendData getActiveResendData() const;

  private:
    void updateAggregateData(uint64_t nodeId, uint64_t nodeStake, std::optional<uint64_t> oldAcknowledgmentValue,
                             std::optional<uint64_t> acknowledgmentValue, std::optional<uint64_t> curQuackValue);
    void updateNodeData(uint64_t nodeId, std::optional<uint64_t> acknowledgmentValue);
    acknowledgment_tracker::ResendData updateActiveResendData();

    // At any instance of time there can only be one message that a node should resend
    // This is because nodes can only be stuck at one quorumAck
    acknowledgment_tracker::ResendData mActiveResendData{};

    const uint64_t kOtherNetworkMaxFailedStake{};
    std::vector<acknowledgment_tracker::NodeAckData> mNodeData;
    std::optional<uint64_t> mCurStuckQuorumAck{};
    uint64_t mCurUnstuckStake{}; // will disable resend data if this > failedStake
    RelaxedQuorumAcknowledgment staleAckQuorumCounter;
};
