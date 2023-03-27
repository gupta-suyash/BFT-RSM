#include "message_scheduler.h"

#include <algorithm>
#include <numeric>

uint64_t message_scheduler::trueMod(int64_t value, int64_t modulus)
{
    const auto remainder = (value % modulus);
    return (remainder < 0) ? remainder + modulus : remainder;
}

std::vector<uint64_t> message_scheduler::getStakePrefixSum(const std::vector<uint64_t> &networkStake)
{
    std::vector<uint64_t> prefixSum(networkStake.size() + 1);

    prefixSum.at(0) = 0;

    for (size_t i = 1; i < prefixSum.size(); i++)
    {
        prefixSum.at(i) = prefixSum.at(i - 1) + networkStake.at(i - 1);
    }

    return prefixSum;
}

uint64_t message_scheduler::stakeToNode(uint64_t stakeIndex, const std::vector<uint64_t> &networkStakePrefixSum)
{
    const auto nodeIterator =
        std::upper_bound(std::cbegin(networkStakePrefixSum), std::cend(networkStakePrefixSum), stakeIndex);
    if (nodeIterator == std::cend(networkStakePrefixSum))
    {
        SPDLOG_CRITICAL("Requested stake that nobody owns, stakeIndex={} totalNetworkStake={}", stakeIndex,
                        networkStakePrefixSum.back());
        std::abort();
    }
    return std::distance(std::cbegin(networkStakePrefixSum), nodeIterator) - 1;
}

uint64_t message_scheduler::nodeToStake(uint64_t nodeIndex, const std::vector<uint64_t> &networkStakePrefixSum)
{
    return networkStakePrefixSum.at(nodeIndex + 1) - networkStakePrefixSum.at(nodeIndex);
}

uint64_t message_scheduler::stakeInNetwork(const std::vector<uint64_t> &networkStakePrefixSum)
{
    return networkStakePrefixSum.back();
}

void message_scheduler::scaleVector(std::vector<uint64_t> &v, uint64_t factor)
{
    for (auto &value : v)
    {
        value *= factor;
    }
}

MessageScheduler::MessageScheduler(NodeConfiguration configuration)
{
    // Scale up stake in both networks to their lcm
    // In the equal stake case we require min(ownNetworkSize, otherNetworkSize) > f1+f2
    // For the stake case we could change this to be min(ownNetworkStake, otherNetworkStake) >= f1+f2+1
    // But consider the case where 4 nodes with 1 stake each talk to 4 nodes with 10 stake each
    // This fails the prior stake condition since we assumed stake had an equal conversion rate
    // Scale up to the LCM changes the condition to be equivalent to:
    //     ownNetworkStake * otherNetworkStake > f1 * otherNetworkStake + f2 * ownNetworkStake
    // Then assuming (f1 < ownNetworkStake / 2), and (f2 < otherNetworkStake / 2) we get
    //     ownNetworkStake * otherNetworkStake > (ownNetworkStake * otherNetworkStake / 2) * 2
    // Which is a true statement. This means if f1, f2 are both < half their network size this class is functional
    auto ownNetworkStakePrefixSum = message_scheduler::getStakePrefixSum(configuration.kOwnNetworkStakes);
    auto otherNetworkStakePrefixSum = message_scheduler::getStakePrefixSum(configuration.kOtherNetworkStakes);
    const auto stakeInOwnNetwork = message_scheduler::stakeInNetwork(ownNetworkStakePrefixSum);
    const auto stakeInOtherNetwork = message_scheduler::stakeInNetwork(otherNetworkStakePrefixSum);
    const auto networkStakeLcm = std::lcm(stakeInOwnNetwork, stakeInOtherNetwork);

    const auto ownNetworkScaleFactor = networkStakeLcm / stakeInOwnNetwork;
    const auto otherNetworkScaleFactor = networkStakeLcm / stakeInOtherNetwork;

    message_scheduler::scaleVector(ownNetworkStakePrefixSum, ownNetworkScaleFactor);
    message_scheduler::scaleVector(otherNetworkStakePrefixSum, otherNetworkScaleFactor);
    const auto scaledOwnNetworkMaxFailedStake = configuration.kOwnMaxNumFailedStake * ownNetworkScaleFactor;
    const auto scaledOtherNetworkMaxFailedStake = configuration.kOtherMaxNumFailedStake * otherNetworkScaleFactor;

    kOwnNodeId = configuration.kNodeId;
    kStakePerRsm = networkStakeLcm;
    kOwnNetworkSize = configuration.kOwnNetworkSize;
    kOtherNetworkSize = configuration.kOtherNetworkSize;
    kOwnMaxNumFailedStake = scaledOwnNetworkMaxFailedStake;
    kOtherMaxNumFailedStake = scaledOtherNetworkMaxFailedStake;
    kMinStakeToSend = scaledOwnNetworkMaxFailedStake + scaledOtherNetworkMaxFailedStake + 1;
    kOwnRsmStakePrefixSum = std::move(ownNetworkStakePrefixSum);
    kOtherRsmStakePrefixSum = std::move(otherNetworkStakePrefixSum);

    bool isImplementationValid = kStakePerRsm >= kOwnMaxNumFailedStake + kOtherMaxNumFailedStake;
    assert(isImplementationValid && "More than half of one of the network's total stake can fail -- MessageScheduler "
                                    "cannot efficently support this");
}

std::optional<uint64_t> MessageScheduler::getResendNumber(uint64_t sequenceNumber) const
{
    const auto roundOffset = sequenceNumber / kStakePerRsm;
    const auto originalSender = sequenceNumber % kStakePerRsm;
    const auto originalReceiver = (sequenceNumber + roundOffset) % kStakePerRsm;
    const auto ownNodeFirstStake = kOwnRsmStakePrefixSum.at(kOwnNodeId);
    const auto ownNodeLastStake = kOwnRsmStakePrefixSum.at(kOwnNodeId + 1) - 1;
    const auto isNodeFirstSender = ownNodeFirstStake <= originalSender && originalSender <= ownNodeLastStake;
    const auto ownNodeFirstSentStake = (isNodeFirstSender) ? originalSender : ownNodeFirstStake;
    const auto previousStakeSent =
        message_scheduler::trueMod((int64_t)ownNodeFirstSentStake - (int64_t)originalSender, kStakePerRsm);

    const auto isOwnNodeNotASender = previousStakeSent >= kMinStakeToSend;
    if (isOwnNodeNotASender)
    {
        return std::nullopt;
    }
    const auto ownNodeFirstReceiver = (originalReceiver + previousStakeSent) % kStakePerRsm;

    const auto originalSenderId = message_scheduler::stakeToNode(originalSender, kOwnRsmStakePrefixSum);
    const auto originalReceiverId = message_scheduler::stakeToNode(originalReceiver, kOtherRsmStakePrefixSum);
    const auto ownFirstSenderId = kOwnNodeId;
    const auto ownFirstReceiverId = message_scheduler::stakeToNode(ownNodeFirstReceiver, kOtherRsmStakePrefixSum);

    const auto priorSenders =
        message_scheduler::trueMod((int64_t)ownFirstSenderId - (int64_t)originalSenderId, kOwnNetworkSize);
    const auto priorReceivers =
        message_scheduler::trueMod((int64_t)ownFirstReceiverId - (int64_t)originalReceiverId, kOtherNetworkSize);

    return std::max(priorSenders, priorReceivers);
}

message_scheduler::CompactDestinationList MessageScheduler::getMessageDestinations(uint64_t sequenceNumber) const
{
    // Algorithm : do all send/recv math with stake, then call stakeToNode
    const auto roundOffset = sequenceNumber / kStakePerRsm;
    const auto originalSender = sequenceNumber % kStakePerRsm;
    const auto originalReceiver = (sequenceNumber + roundOffset) % kStakePerRsm;
    const auto finalSender = (sequenceNumber + kMinStakeToSend - 1) % kStakePerRsm;
    const auto ownNodeFirstStake = kOwnRsmStakePrefixSum.at(kOwnNodeId);
    const auto ownNodeLastStake = kOwnRsmStakePrefixSum.at(kOwnNodeId + 1) - 1;
    const auto isNodeFirstSender = ownNodeFirstStake <= originalSender && originalSender <= ownNodeLastStake;
    const auto ownNodeFirstSentStake = (isNodeFirstSender) ? originalSender : ownNodeFirstStake;
    const auto previousStakeSent =
        message_scheduler::trueMod((int64_t)ownNodeFirstSentStake - (int64_t)originalSender, kStakePerRsm);

    const auto isOwnNodeNotASender = previousStakeSent >= kMinStakeToSend;
    if (isOwnNodeNotASender)
    {
        return message_scheduler::CompactDestinationList{};
    }

    const auto isNodeCutoff = (ownNodeFirstSentStake <= finalSender && finalSender < ownNodeLastStake);
    const auto ownNodeFinalSentStake = (isNodeCutoff) ? finalSender : ownNodeLastStake;
    const auto stakeSentByOwnNode = ownNodeFinalSentStake - ownNodeFirstSentStake + 1;
    message_scheduler::CompactDestinationList destinations{};

    int64_t stakeLeftToSend = stakeSentByOwnNode;
    auto curReceiverStake = (originalReceiver + previousStakeSent) % kStakePerRsm;
    auto curReceiverId = message_scheduler::stakeToNode(curReceiverStake, kOtherRsmStakePrefixSum);
    while (stakeLeftToSend > 0)
    {
        // using uint16_t is a petty optimization and can be removed anytime :whistling:
        destinations.push_back((uint16_t)curReceiverId);

        const auto stakeSentToCurReceiver = kOtherRsmStakePrefixSum.at(curReceiverId + 1) - curReceiverStake;

        stakeLeftToSend -= stakeSentToCurReceiver;
        curReceiverStake = (curReceiverStake + stakeSentToCurReceiver) % kStakePerRsm;
        curReceiverId = (curReceiverId + 1 == kOtherNetworkSize) ? 0 : curReceiverId + 1;
    }
    return destinations;
}
