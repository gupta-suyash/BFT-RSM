#include "global.h"

#include <optional>
#include <vector>

#include <boost/container/small_vector.hpp>

namespace message_scheduler
{
using CompactDestinationList = boost::container::small_vector<uint16_t, 8>;

uint64_t trueMod(int64_t value, int64_t modulus);

// stakePrefixSum[0] = 0
// stakePrefixSum[i+1] = networkStake[0] + networkStake[1] + ... + networkStake[i]
// stakePrefixSum[i] = "id" of node i's first piece of stake
std::vector<uint64_t> getStakePrefixSum(const std::vector<uint64_t> &networkStake);

uint64_t stakeToNode(uint64_t stakeIndex, const std::vector<uint64_t> &networkStakePrefixSum);
uint64_t nodeToStake(uint64_t nodeIndex, const std::vector<uint64_t> &networkStakePrefixSum);
uint64_t stakeInNetwork(const std::vector<uint64_t> &networkStakePrefixSum);
void scaleVector(std::vector<uint64_t> &v, uint64_t factor);
}; // namespace message_scheduler

class MessageScheduler
{
  public:
    MessageScheduler(NodeConfiguration configuration);
    std::optional<uint64_t> getResendNumber(uint64_t sequenceNumber) const;
    message_scheduler::CompactDestinationList getMessageDestinations(uint64_t sequenceNumber) const;
    // TODO: Add method to detect if node should receive a certain message (only slightly helps to filter byzantine
    // attacks)
    //       Implementation plan: just reverse own/other variables, and check nodeid \in?
    //       getMessageDestinations(senderId)

  private:
    uint64_t kOwnNodeId{};
    uint64_t kStakePerRsm{};
    uint64_t kOwnNetworkSize{};
    uint64_t kOtherNetworkSize{};
    uint64_t kOwnMaxNumFailedStake{};
    uint64_t kOtherMaxNumFailedStake{};
    uint64_t kMinStakeToSend{};
    std::vector<uint64_t> kOwnRsmStakePrefixSum{};
    std::vector<uint64_t> kOtherRsmStakePrefixSum{};
};
