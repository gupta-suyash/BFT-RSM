#include "message_scheduler.h"

#include <algorithm>
#include <cmath>
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
        std::find_if(std::cbegin(networkStakePrefixSum), std::cend(networkStakePrefixSum), [stakeIndex](const uint64_t x)
        {
            return stakeIndex < x;
        });
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
    if (nodeIndex >= networkStakePrefixSum.size())
    {
        SPDLOG_CRITICAL("Requested node that doesn't exist, nodeIndex={}", nodeIndex);
        std::abort();
    }
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

std::vector<uint64_t> message_scheduler::apportionVector(uint64_t totalApportionedShares,
                                                         const std::vector<uint64_t> &originalShares)
{
    std::vector<uint64_t> apportionedShares;
    std::vector<std::pair<double, uint64_t>> roundingErrToOwner;
    uint64_t sharesAlreadyApportioned{};

    const auto totalOriginalShares = std::accumulate(std::cbegin(originalShares), std::cend(originalShares), 0);

    for (uint64_t i = 0; i < originalShares.size(); i++)
    {
        const auto curShare = originalShares.at(i);
        const auto curOwner = i;

        const double idealShare = curShare / (double)totalOriginalShares * totalApportionedShares;

        const uint64_t minimalShare = (uint64_t)idealShare;
        const auto remainder = fmod(idealShare, 1);

        sharesAlreadyApportioned += minimalShare;
        apportionedShares.push_back(minimalShare);
        roundingErrToOwner.push_back({remainder, curOwner});
    }

    std::sort(std::begin(roundingErrToOwner), std::end(roundingErrToOwner));

    for (uint64_t i{}; sharesAlreadyApportioned < totalApportionedShares; i++)
    {
        const auto [remainder, owner] = roundingErrToOwner.at(i);

        apportionedShares.at(owner)++;
        sharesAlreadyApportioned++;
    }

    for (auto i : apportionedShares)
    {
        assert(i > 0 && "fix apportionment scheduler to round to positive integers");
    }

    assert(std::accumulate(std::cbegin(apportionedShares), std::cend(apportionedShares), 0) == totalApportionedShares);

    return apportionedShares;
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
    const auto ownApportionedStake = stakeInOwnNetwork;
    const auto otherApportionedStake = stakeInOtherNetwork;
    const auto ownNetworkApportionedStakes =
        message_scheduler::apportionVector(ownApportionedStake, configuration.kOwnNetworkStakes);
    const auto otherNetworkApportionedStakes =
        message_scheduler::apportionVector(otherApportionedStake, configuration.kOtherNetworkStakes);

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
    kOwnApportionedStake = ownApportionedStake;
    kOtherApportionedStake = otherApportionedStake;
    kOwnRsmApportionedStakePrefixSum = message_scheduler::getStakePrefixSum(ownNetworkApportionedStakes);
    kOtherRsmApportionedStakePrefixSum = message_scheduler::getStakePrefixSum(otherNetworkApportionedStakes);

    bool isImplementationValid = kStakePerRsm >= kOwnMaxNumFailedStake + kOtherMaxNumFailedStake;
    assert(isImplementationValid &&
           "More than half of one of the network's total stake can fail -- probably works but not tested");

    mResendNumberLookup = std::vector<std::vector<std::optional<ResendNumberType>>>(kOtherNetworkSize,
            std::vector<std::optional<ResendNumberType>>(kOtherNetworkSize)
        );
    mResendDestinationLookup = std::vector<std::vector<std::optional<message_scheduler::CompactDestinationList>>>(kOwnNetworkSize, std::vector<std::optional<message_scheduler::CompactDestinationList>>(kOtherNetworkSize));
}

std::optional<uint64_t> MessageScheduler::getResendNumber(uint64_t sequenceNumber) const
{
    const auto roundOffset = (sequenceNumber / kOwnApportionedStake) % (kOtherApportionedStake - 1);
    const auto originalApportionedSendNode = sequenceNumber % kOwnApportionedStake;

    auto lookupEntry = &mResendNumberLookup[originalApportionedSendNode][roundOffset];

    if (lookupEntry->has_value())
    {
        return lookupEntry->value();
    }

    if (originalApportionedSendNode == kOwnNodeId)
    {
        *lookupEntry = 0;
        return 0;
    }

    const auto firstResender = (originalApportionedSendNode + 1 + roundOffset + (1 + roundOffset)/kOwnApportionedStake)%kOwnApportionedStake;
    if (firstResender == kOwnNodeId)
    {
        *lookupEntry = 1;
        return 1;
    }

    const auto secondResender = (originalApportionedSendNode + 2 + roundOffset + (2 + roundOffset)/kOwnApportionedStake)%kOwnApportionedStake;
    if (secondResender == kOwnNodeId)
    {
        *lookupEntry = 2;
        return 2;
    }
    *lookupEntry = std::optional<uint64_t>{};
    return std::nullopt;
}

message_scheduler::CompactDestinationList MessageScheduler::getMessageDestinations(uint64_t sequenceNumber) const
{
    const auto roundOffset = sequenceNumber / kOwnApportionedStake;
    const auto originalApportionedSendNode =
        message_scheduler::stakeToNode(sequenceNumber % kOwnApportionedStake, kOwnRsmApportionedStakePrefixSum);
    const auto originalApportionedRecvNode = message_scheduler::stakeToNode(
        (sequenceNumber + roundOffset) % kOtherApportionedStake, kOtherRsmApportionedStakePrefixSum);

    std::optional<message_scheduler::CompactDestinationList>* const lookupEntry = mResendDestinationLookup[originalApportionedSendNode].data() + originalApportionedRecvNode;

    if (lookupEntry->has_value())
    {
        return lookupEntry->value();
    }

    const auto resendNum = getResendNumber(sequenceNumber);
    if (not resendNum.has_value())
    {
        *lookupEntry = message_scheduler::CompactDestinationList{};
        return lookupEntry->value();
    }
    const auto destination = (originalApportionedRecvNode + resendNum.value())%4;

    *lookupEntry = message_scheduler::CompactDestinationList{(uint16_t)destination};
    return lookupEntry->value();
}
