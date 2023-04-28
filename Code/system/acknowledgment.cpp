#include "acknowledgment.h"

/* Adds an stores the new ack value in mAckWindows.
 * Consecutive acks are stored with sliding windows.
 *
 * @param mid is the value to be added
 */
void Acknowledgment::addToAckList(const uint64_t ack)
{
    const auto curAckValue = mAckValue.load(std::memory_order::relaxed);
    if (ack <= curAckValue)
    {
        return;
    }
    const auto ackLocation = ack & (kWindowSize - 1);
    mAckWindow[ackLocation] = true;


    auto curWindowBaseline = curAckValue.value_or(0ULL - 1) + 1;
    while (mAckWindow[curWindowBaseline])
    {
        mAckWindow[curWindowBaseline] = false;
        curWindowBaseline = (curWindowBaseline + 1 == kWindowSize)? 0 : curWindowBaseline + 1;
    }
    const auto highestAcked = curWindowBaseline - 1;
    if (highestAcked != curAckValue.value_or(0ULL - 1))
    {
        mAckValue.store(highestAcked, std::memory_order::release);
    }
}

/* Get the value of variable ackValue; needs to be locked as multi-threaded access.
 *
 * @return mAckValue, the current highest received acknowledged value
 */
std::optional<uint64_t> Acknowledgment::getAckIterator() const
{
    return mAckValue.load(std::memory_order::acquire);
}
