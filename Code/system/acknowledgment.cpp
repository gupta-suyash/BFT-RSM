#include "acknowledgment.h"

/* Adds an stores the new ack value in mAckWindows.
 * Consecutive acks are stored with sliding windows.
 *
 * @param mid is the value to be added
 */
void Acknowledgment::addToAckList(const uint64_t ack)
{
    static constexpr uint64_t kAckBatch{2048};
    thread_local std::vector<uint64_t> localAcks{};
    thread_local uint64_t curRound{};

    if (curRound++ < kAckBatch)
    {
        localAcks.push_back(ack);
    }

    curRound = 0;

    std::scoped_lock lock{mMutex};
    for (const auto ack : localAcks)
    {
        mAckWindows.add(ack);
    }

    localAcks.clear();

    const auto minimumAckWindow = std::cbegin(mAckWindows);

    if (minimumAckWindow->lower() <= kMinimumAckValue)
    {
        mAckValue.store(minimumAckWindow->upper(), std::memory_order::release);
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
