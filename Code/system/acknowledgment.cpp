#include "acknowledgment.h"

/* Adds an stores the new ack value in mAckWindows.
 * Consecutive acks are stored with sliding windows.
 *
 * @param mid is the value to be added
 */
void Acknowledgment::addToAckList(const uint64_t ack)
{
    // std::scoped_lock lock{mMutex};
    const auto curAckValue = mAckValue.load(std::memory_order_relaxed);
    if (ack <= curAckValue)
    {
        return;
    }
    const auto ackLocation = ack & (kWindowSize - 1);
    mAckWindow[ackLocation / 64] |= 1ULL << (ackLocation % 64);

    auto curWindowBaseline = curAckValue.value_or(0ULL - 1) + 1;
    while (mAckWindow[curWindowBaseline / 64] & (1ULL << (curWindowBaseline % 64)))
    {
        // mAckWindow[curWindowBaseline / 64] ^= 1ULL << (curWindowBaseline % 64);
        curWindowBaseline = (curWindowBaseline + 1 == kWindowSize) ? 0 : curWindowBaseline + 1;
    }
    const auto highestAcked = curWindowBaseline - 1;
    if (highestAcked != curAckValue.value_or(0ULL - 1))
    {
        mAckValue.store(highestAcked, std::memory_order_release);
    }
}

/* Get the value of variable ackValue; needs to be locked as multi-threaded access.
 *
 * @return mAckValue, the current highest received acknowledged value
 */
std::optional<uint64_t> Acknowledgment::getAckIterator() const
{
    // std::scoped_lock lock{mMutex};
    return mAckValue.load(std::memory_order_acquire);
}

bool Acknowledgment::testAck(uint64_t test) const
{
    // technically doesn't work if querying > kWindowSize messages in the future ~3Billion messages
    return mAckWindow[test / 64] & (1ULL << (test % 64));
}
