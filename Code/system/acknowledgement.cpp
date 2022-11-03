#include "acknowledgement.h"

/* Adds an stores the new ack value in mAckWindows.
 * Consecutive acks are stored with sliding windows.
 *
 * @param mid is the value to be added
 */
void Acknowledgment::addToAckList(const uint64_t nodeId)
{
    // Need to lock accesses to ackValue as it used by multiple threads
    std::scoped_lock lock{mMutex};

    mAckWindows.add(nodeId);

    const auto minimumAckWindow = std::cbegin(mAckWindows);

    if (minimumAckWindow->lower() <= kMinimumAckValue)
    {
        mAckValue = minimumAckWindow->upper();
    }
}

/* Get the value of variable ackValue; needs to be locked as multi-threaded access.
 *
 * @return mAckValue, the current highest received acknowledged value
 */
std::optional<uint64_t> Acknowledgment::getAckIterator() const
{
    // Need to lock accesses to ackValue as it used by multiple threads
    std::scoped_lock lock{mMutex};
    return mAckValue;
}
