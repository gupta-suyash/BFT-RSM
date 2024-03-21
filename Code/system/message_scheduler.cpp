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
    for (size_t offset{1};; offset++)
    {
        if (stakeIndex < networkStakePrefixSum[offset])
        {
            return offset - 1;
        }
    }
}

uint64_t message_scheduler::stakeInNode(uint64_t nodeIndex, const std::vector<uint64_t> &networkStakePrefixSum)
{
    return networkStakePrefixSum[nodeIndex + 1] - networkStakePrefixSum[nodeIndex];
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

static uint64_t bitCeil(uint64_t value)
{
    value--; // if value is already a power of 2
    value |= value >> 1; // paint highest bit everywhere
    value |= value >> 2;
    value |= value >> 4;
    value |= value >> 8;
    value |= value >> 16;
    value |= value >> 32;
    value++; // add one to find pow2 > value - 1
    return value;
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


std::optional<uint64_t> MessageScheduler::computeGetResendNumber(uint64_t sequenceNumber) const
{
    const auto originalSender = sequenceNumber % kOwnApportionedStake;
    sequenceNumber /= kOwnApportionedStake;
    const auto originalReceiverOffset = sequenceNumber % kOtherApportionedStake;
    sequenceNumber /= kOtherApportionedStake;
    const auto resendOffset = sequenceNumber % (kOwnApportionedStake - 1);

    // All Apportioned Stake Sender/Receivers
    const auto firstSender = originalSender;
    const auto firstReceiver = (originalSender + originalReceiverOffset) % kOtherApportionedStake;
    const auto firstReSender = (firstSender + resendOffset + 1) % kOwnApportionedStake;
    const auto firstReReceiver = (firstReceiver + resendOffset + 1) % kOtherApportionedStake;

    // Actual Node Ids
    const auto firstSenderId = message_scheduler::stakeToNode(
        firstSender, kOwnRsmApportionedStakePrefixSum);
    const auto firstReSenderId = message_scheduler::stakeToNode(
        firstReSender, kOwnRsmApportionedStakePrefixSum);
    const auto firstReceiverId = message_scheduler::stakeToNode(
        firstReceiver, kOtherRsmApportionedStakePrefixSum);
    const auto firstReReceiverId = message_scheduler::stakeToNode(
        firstReReceiver, kOtherRsmApportionedStakePrefixSum);

    bool isNodeFirstSender = firstSenderId == kOwnNodeId;
    if (isNodeFirstSender)
    {
        return 0;
    }

    // Optimistic starting resender/rereceiver
    int64_t stakeLeftInFirstReceiver = message_scheduler::stakeInNode(firstReceiverId, kOtherRsmStakePrefixSum);
    uint64_t curSendNodeId = firstReSenderId;
    uint64_t curRecvNodeId = firstReReceiverId;
    int64_t stakeLeftInSender = message_scheduler::stakeInNode(curSendNodeId, kOwnRsmStakePrefixSum);
    int64_t stakeLeftInReceiver = message_scheduler::stakeInNode(curRecvNodeId, kOtherRsmStakePrefixSum);
    uint64_t curResendNumber = 1;

    const auto stakeSentInFirstMessage = std::min(
        message_scheduler::stakeInNode(firstSenderId, kOwnRsmStakePrefixSum),
        message_scheduler::stakeInNode(firstReceiverId, kOtherRsmStakePrefixSum));
    stakeLeftInFirstReceiver -= stakeSentInFirstMessage;

    int64_t remainingStakeToSend = (int64_t) kMinStakeToSend - (int64_t) stakeSentInFirstMessage;

    if (curSendNodeId == firstSenderId)
    {
        stakeLeftInSender -= stakeSentInFirstMessage;
    }
    if (curRecvNodeId == firstReceiverId)
    {
        stakeLeftInReceiver -= stakeSentInFirstMessage;
    }

    // Simulate Sends -- skipping over firstSenderId and firstResenderId <- kind of a bug but dw about it -- current types can't handle gaps in resends
    // We will still send enough stake in any case that this function halts :-)
    // Just harder to see if the function halts in all cases or not ...
    while (remainingStakeToSend > 0 && (curSendNodeId == firstSenderId))
    {
        while (stakeLeftInSender == 0)
        {
            curSendNodeId = (curSendNodeId + 1) % kOwnNetworkSize;
            stakeLeftInSender = message_scheduler::stakeInNode(curSendNodeId, kOwnRsmStakePrefixSum);
        }
        while (stakeLeftInReceiver == 0)
        {
            curRecvNodeId = (curRecvNodeId + 1) % kOtherNetworkSize;
            stakeLeftInReceiver = (curRecvNodeId == firstReceiverId)? stakeLeftInFirstReceiver : message_scheduler::stakeInNode(curRecvNodeId, kOtherRsmStakePrefixSum);
        }


        if (curSendNodeId == kOwnNodeId)
        {
            // I'm a sender ! I did resend curResendNumber first! -- doesn't occur in this case but w/e
            return curResendNumber;
        }
        const auto stakeSentInSimulatedMessage = std::min(stakeLeftInSender, stakeLeftInReceiver);
        stakeLeftInReceiver -= stakeSentInSimulatedMessage;
        stakeLeftInFirstReceiver = (curRecvNodeId == firstReceiverId)? stakeLeftInReceiver : stakeLeftInFirstReceiver;
        stakeLeftInSender -= stakeSentInSimulatedMessage;
        curResendNumber++;
        remainingStakeToSend -= stakeSentInSimulatedMessage;
    }

    while (remainingStakeToSend > 0)
    {
        while (stakeLeftInSender == 0 || curSendNodeId == firstSenderId)
        {
            curSendNodeId = (curSendNodeId + 1) % kOwnNetworkSize;
            stakeLeftInSender = message_scheduler::stakeInNode(curSendNodeId, kOwnRsmStakePrefixSum);
        }
        while (stakeLeftInReceiver == 0)
        {
            curRecvNodeId = (curRecvNodeId + 1) % kOtherNetworkSize;
            stakeLeftInReceiver = (curRecvNodeId == firstReceiverId)? stakeLeftInFirstReceiver : message_scheduler::stakeInNode(curRecvNodeId, kOtherRsmStakePrefixSum);
        }

        if (curSendNodeId == kOwnNodeId)
        {
            // I'm a sender ! I did resend curResendNumber first!
            return curResendNumber;
        }
        const auto stakeSentInSimulatedMessage = std::min(stakeLeftInSender, stakeLeftInReceiver);
        stakeLeftInReceiver -= stakeSentInSimulatedMessage;
        stakeLeftInFirstReceiver = (curRecvNodeId == firstReceiverId)? stakeLeftInReceiver : stakeLeftInFirstReceiver;
        stakeLeftInSender -= stakeSentInSimulatedMessage;
        remainingStakeToSend -= stakeSentInSimulatedMessage;
        curResendNumber++;
    }

    // Didn't use the current node to send once in the simulation.
    return std::nullopt;
}

message_scheduler::CompactDestinationList MessageScheduler::computeGetMessageDestinations(uint64_t sequenceNumber) const
{
    const auto originalSender = sequenceNumber % kOwnApportionedStake;
    sequenceNumber /= kOwnApportionedStake;
    const auto originalReceiverOffset = sequenceNumber % kOtherApportionedStake;
    sequenceNumber /= kOtherApportionedStake;
    const auto resendOffset = sequenceNumber % (kOwnApportionedStake - 1);

    // All Apportioned Stake Sender/Receivers
    const auto firstSender = originalSender;
    const auto firstReceiver = (originalSender + originalReceiverOffset) % kOtherApportionedStake;
    const auto firstReSender = (firstSender + resendOffset + 1) % kOwnApportionedStake;
    const auto firstReReceiver = (firstReceiver + resendOffset + 1) % kOtherApportionedStake;

    // Actual Node Ids
    const auto firstSenderId = message_scheduler::stakeToNode(
        firstSender, kOwnRsmApportionedStakePrefixSum);
    const auto firstReSenderId = message_scheduler::stakeToNode(
        firstReSender, kOwnRsmApportionedStakePrefixSum);
    const auto firstReceiverId = message_scheduler::stakeToNode(
        firstReceiver, kOtherRsmApportionedStakePrefixSum);
    const auto firstReReceiverId = message_scheduler::stakeToNode(
        firstReReceiver, kOtherRsmApportionedStakePrefixSum);

    message_scheduler::CompactDestinationList destinations{};

    bool isNodeFirstSender = firstSenderId == kOwnNodeId;
    if (isNodeFirstSender)
    {
        destinations.push_back((uint16_t) firstReceiverId);
    }

    // Optimistic starting resender/rereceiver
    int64_t stakeLeftInFirstReceiver = message_scheduler::stakeInNode(firstReceiverId, kOtherRsmStakePrefixSum);
    uint64_t curSendNodeId = firstReSenderId;
    uint64_t curRecvNodeId = firstReReceiverId;
    int64_t stakeLeftInSender = message_scheduler::stakeInNode(curSendNodeId, kOwnRsmStakePrefixSum);
    int64_t stakeLeftInReceiver = message_scheduler::stakeInNode(curRecvNodeId, kOtherRsmStakePrefixSum);
    uint64_t curResendNumber = 1;

    const auto stakeSentInFirstMessage = std::min(
        message_scheduler::stakeInNode(firstSenderId, kOwnRsmStakePrefixSum),
        message_scheduler::stakeInNode(firstReceiverId, kOtherRsmStakePrefixSum));
    stakeLeftInFirstReceiver -= stakeSentInFirstMessage;

    int64_t remainingStakeToSend = (int64_t) kMinStakeToSend - (int64_t) stakeSentInFirstMessage;

    if (curSendNodeId == firstSenderId)
    {
        stakeLeftInSender -= stakeSentInFirstMessage;
    }
    if (curRecvNodeId == firstReceiverId)
    {
        stakeLeftInReceiver = stakeLeftInFirstReceiver;
    }

    // Simulate Sends -- skipping over firstSenderId and firstResenderId <- kind of a bug but dw about it -- current types can't handle gaps in resends
    // We will still send enough stake in any case that this function halts :-)
    // Just harder to see if the function halts in all cases or not ...
    while (remainingStakeToSend > 0 && (curSendNodeId == firstSenderId))
    {
        while (stakeLeftInSender == 0)
        {
            curSendNodeId = (curSendNodeId + 1) % kOwnNetworkSize;
            stakeLeftInSender = message_scheduler::stakeInNode(curSendNodeId, kOwnRsmStakePrefixSum);
        }
        while (stakeLeftInReceiver == 0)
        {
            curRecvNodeId = (curRecvNodeId + 1) % kOtherNetworkSize;
            stakeLeftInReceiver = (curRecvNodeId == firstReceiverId)? stakeLeftInFirstReceiver : message_scheduler::stakeInNode(curRecvNodeId, kOtherRsmStakePrefixSum);
        }


        if (curSendNodeId == kOwnNodeId)
        {
            // I'm a sender ! I sent to curRecvNodeId
            destinations.push_back((uint16_t) curRecvNodeId);
        }
        const auto stakeSentInSimulatedMessage = std::min(stakeLeftInSender, stakeLeftInReceiver);
        stakeLeftInReceiver -= stakeSentInSimulatedMessage;
        stakeLeftInFirstReceiver = (curRecvNodeId == firstReceiverId)? stakeLeftInReceiver : stakeLeftInFirstReceiver;
        stakeLeftInSender -= stakeSentInSimulatedMessage;
        remainingStakeToSend -= stakeSentInSimulatedMessage;
        curResendNumber++;
    }

    while (remainingStakeToSend > 0)
    {
        while (stakeLeftInSender == 0 || curSendNodeId == firstSenderId)
        {
            curSendNodeId = (curSendNodeId + 1) % kOwnNetworkSize;
            stakeLeftInSender = message_scheduler::stakeInNode(curSendNodeId, kOwnRsmStakePrefixSum);
        }
        while (stakeLeftInReceiver == 0)
        {
            curRecvNodeId = (curRecvNodeId + 1) % kOtherNetworkSize;
            stakeLeftInReceiver = (curRecvNodeId == firstReceiverId)? stakeLeftInFirstReceiver : message_scheduler::stakeInNode(curRecvNodeId, kOtherRsmStakePrefixSum);
        }

        if (curSendNodeId == kOwnNodeId)
        {
            // I'm a sender ! I sent to curRecvNodeId
            destinations.push_back((uint16_t) curRecvNodeId);
        }
        const auto stakeSentInSimulatedMessage = std::min(stakeLeftInSender, stakeLeftInReceiver);
        stakeLeftInReceiver -= stakeSentInSimulatedMessage;
        stakeLeftInFirstReceiver = (curRecvNodeId == firstReceiverId)? stakeLeftInReceiver : stakeLeftInFirstReceiver;
        stakeLeftInSender -= stakeSentInSimulatedMessage;
        remainingStakeToSend -= stakeSentInSimulatedMessage;
        curResendNumber++;
    }

    // Didn't use the current node to send once in the simulation.
    return destinations;
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
    const auto ownApportionedStake = std::min<uint64_t>(networkStakeLcm, 1024);
    const auto otherApportionedStake = std::min<uint64_t>(networkStakeLcm, 1024);
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
    const auto schedulingCycleLength = kOwnApportionedStake * kOtherApportionedStake * (kOwnApportionedStake - 1);
    const auto vectorSizePow2 = bitCeil(schedulingCycleLength);
    kCycleMask = vectorSizePow2 - 1;
    for (int sequenceNumber{}; sequenceNumber < vectorSizePow2; sequenceNumber++)
    {
        mResendNumberLookup.push_back(computeGetResendNumber(sequenceNumber));
        mResendDestinationLookup.push_back(computeGetMessageDestinations(sequenceNumber));
        // std::string resendDests{};
        // for (const auto& destination : mResendDestinationLookup.back())
        // {
        //     resendDests += std::to_string(destination) + ' ';
        // }
        // if (mResendNumberLookup.back().has_value())
        // {
        //     SPDLOG_CRITICAL("FOR SN {}\t I AM RESENDER {} to [ {}]", sequenceNumber, mResendNumberLookup.back().value(), resendDests);
        // }
        // else
        // {
        //     SPDLOG_CRITICAL("FOR SN {}\t I AM RESENDER NA to [ {}]", sequenceNumber, resendDests);
        // }
    }
}

std::optional<uint64_t> MessageScheduler::getResendNumber(uint64_t sequenceNumber) const
{
    return mResendNumberLookup[sequenceNumber & kCycleMask];
}

message_scheduler::CompactDestinationList MessageScheduler::getMessageDestinations(uint64_t sequenceNumber) const
{
    return mResendDestinationLookup[sequenceNumber & kCycleMask];
}

uint64_t MessageScheduler::getMessageCycleLength() const
{
    return std::max(mResendNumberLookup.size(), mResendDestinationLookup.size());
}
