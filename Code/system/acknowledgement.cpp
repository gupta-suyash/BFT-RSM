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

    const auto minimumAck = std::cbegin(mAckWindows)->lower();

    if (!mAckValue.has_value() && minimumAck == 0)
    {
        // Acks start counting up from 0
        mAckValue = 0;
    }
    else if (mAckValue.value() + 1 == minimumAck)
    {
        mAckValue = minimumAck;
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
