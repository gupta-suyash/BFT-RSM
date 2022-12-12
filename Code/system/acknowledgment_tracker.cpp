#include "acknowledgment_tracker.h"

/* Records that nodeId reported a cummulative acknoledgement of messages [1,...,acknowledgmentValue] at time curTime
 */
void AcknowledgmentTracker::updateNodeData(const uint64_t nodeId, const uint64_t acknowledgmentValue,
                                           const std::chrono::steady_clock::time_point curTime)
{
    const std::scoped_lock lock{mMutex};

    auto nodeDataEntry = mNodeData.find(nodeId);

    const bool isNewNode = nodeDataEntry == std::end(mNodeData);
    if (isNewNode)
    {
        auto nodeData = acknowledgment_tracker::NodeAckData{
            .acknowledgmentValue = acknowledgmentValue, .repeatNumber = 1, .initialAckTime = curTime};
        incrementAggregates(nodeData);
        mNodeData.insert({nodeId, std::move(nodeData)});
        return;
    }

    auto &storedNodeData = nodeDataEntry->second;

    bool isByzantine = storedNodeData.acknowledgmentValue > acknowledgmentValue;
    if (isByzantine)
    {
        // ignore, treat as if the node was crashed
        return;
    }

    bool isStuck = storedNodeData.acknowledgmentValue == acknowledgmentValue;
    if (isStuck)
    {
        const auto stuckAckValue = acknowledgmentValue;
        const auto oldNodeIniitalAckTime = storedNodeData.initialAckTime;
        const auto oldAggregateInitialAckTime = mAggregateInitialAckTime[stuckAckValue];

        storedNodeData.initialAckTime = std::min(oldNodeIniitalAckTime, curTime);
        storedNodeData.repeatNumber++;

        mAggregateInitialAckTime[stuckAckValue] = std::min(oldAggregateInitialAckTime, storedNodeData.initialAckTime);
        mAggregateAckCount[storedNodeData.acknowledgmentValue]++;
        return;
    }

    // Ack value is increasing, remove from aggregates
    decrementAggregates(storedNodeData);

    // Update stored data
    storedNodeData.acknowledgmentValue = acknowledgmentValue;
    storedNodeData.repeatNumber = 1;
    storedNodeData.initialAckTime = curTime;

    // Re-include updated data from aggregates
    incrementAggregates(storedNodeData);
}

// Updates aggregates to totally remove a once included NodeAckData
void AcknowledgmentTracker::decrementAggregates(const acknowledgment_tracker::NodeAckData &oldAckData)
{
    const std::scoped_lock lock{mMutex};

    const auto &[oldAckValue, oldRepeatNumber, oldInitTime] = oldAckData;

    // Ack value is increasing, reset data structures
    const auto remainingAggregateAcks = mAggregateAckCount[oldAckValue] - oldRepeatNumber;
    if (remainingAggregateAcks == 0)
    {
        // delete old stats, no nodes remaining at value
        mAggregateAckCount.erase(oldAckValue);
        mAggregateInitialAckTime.erase(oldAckValue);
    }
    else
    {
        mAggregateAckCount[oldAckValue] -= oldRepeatNumber;
        // mAggregateInitialAckTime[storedAckValue] should increase probably?
    }
}

// Updates aggregates to include a totally new NodeAckData being inserted at once
void AcknowledgmentTracker::incrementAggregates(const acknowledgment_tracker::NodeAckData &newNodeData)
{
    const std::scoped_lock lock{mMutex};

    const auto &[newAckValue, newRepeatNumber, newInitTime] = newNodeData;

    mAggregateAckCount[newAckValue] += newRepeatNumber;

    const auto curFirstAckTimeEntry = mAggregateInitialAckTime.find(newAckValue);
    if (curFirstAckTimeEntry == std::end(mAggregateInitialAckTime))
    {
        mAggregateInitialAckTime.insert({newAckValue, newInitTime});
    }
    else
    {
        const auto oldFirstAckTime = curFirstAckTimeEntry->second;
        mAggregateInitialAckTime[newAckValue] = std::min(oldFirstAckTime, newInitTime);
    }
}

/* Get the first time nodes became stuck at acknowledgmentValuethe aggregate minimum time that a node reported
 * acknowledgmentValue If no nodes are stuck at acknowledgmentValue, it returns std::nullopt
 * TODO Should this value increase when a node gets unstuck?
 * @param acknowledgmentValue the acknowledgment value where nodes may be 'stuck' at
 * @returns of all nodes N who's newest acknowledgment is acknowledgmentValue, return the aggregate minimum time of when
 * N acknowedged acknowledgmentValue if no node's newest acknowledgment is acknowledgmentValue return std::nullopt
 */
std::optional<std::chrono::steady_clock::time_point> AcknowledgmentTracker::getAggregateInitialAckTime(
    uint64_t acknowledgmentValue) const
{
    const std::scoped_lock lock{mMutex};
    const auto ackTimeEntry = mAggregateInitialAckTime.find(acknowledgmentValue);

    if (ackTimeEntry == std::end(mAggregateInitialAckTime))
    {
        return std::nullopt;
    }
    return ackTimeEntry->second;
}

/* Get the aggregate number of times nodes have reported that they are currently stuck at acknowledgmentValue
 * @param acknowledgmentValue the acknowledgment value where nodes may be 'stuck' at
 * @returns of all nodes N who's newest acknowledgment is acknowledgmentValue, return the aggregate sum of how many
 * times N acknowedged acknowledgmentValue
 */
uint64_t AcknowledgmentTracker::getAggregateRepeatedAckCount(uint64_t acknowledgmentValue) const
{
    const std::scoped_lock lock{mMutex};
    const auto ackCountEntry = mAggregateAckCount.find(acknowledgmentValue);

    if (ackCountEntry == std::end(mAggregateAckCount))
    {
        return 0;
    }
    return ackCountEntry->second;
}
