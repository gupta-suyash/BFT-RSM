#include "pipe_queue.h"
#include <optional>

/* Computes the time when curNodeId should resend a message received at receiveTime.
 *
 * @param resendWaitTime is the amount of time put between each sender's transmission to reduce prevent simultanious
 * message transmission
 * @param maxNumSendersPerMsg is the worst case number of nodes who should send each message to ensure delivery,
 * normally 2f+1 (f bad senders + f bad receivers + 1 good pair -- no overlap between cases)
 */
std::optional<std::chrono::steady_clock::time_point> getSendTime(
    const scrooge::CrossChainMessage &message, const std::chrono::steady_clock::time_point receiveTime,
    const std::chrono::steady_clock::time_point::duration resendWaitTime, const uint64_t curNodeId,
    const uint64_t ownNetworkSize, const uint64_t maxNumSendersPerMsg)
{
    const auto trueMod = [](int64_t v, int64_t m) -> uint64_t { return ((v % m) + m) % m; };
    const auto originalSenderId = message.data().sequence_number() % ownNetworkSize;
    const auto nodeDiff = trueMod(curNodeId - originalSenderId, ownNetworkSize);
    if (nodeDiff > maxNumSendersPerMsg)
    {
        return std::nullopt;
    }

    return receiveTime + nodeDiff * resendWaitTime;
}

bool compareByResend(const pipe_queue::TimestampedMessage &l, const pipe_queue::TimestampedMessage &r)
{
    return l.sendTime > r.sendTime;
}

PipeQueue::PipeQueue(const uint64_t curNodeId, const uint64_t ownNetworkSize, const uint64_t maxNumSendersPerMsg,
                     const std::chrono::steady_clock::time_point::duration waitTime)
    : mCurNodeId(curNodeId), mOwnNetworkSize(ownNetworkSize), mMaxNumSendersPerMsg(maxNumSendersPerMsg),
      mWaitTime(waitTime)
{
}

/* This function is used to enqueue a message received from the protocol running
 * at the node. If this message should be sent by the current node, then it will be stored and can be retreived when
 * this node should send it. Nodes select which message to send based on the mod of the message's sequence_id and number
 * of nodes in its own RSM.
 */
void PipeQueue::addMessage(scrooge::CrossChainMessage &&message, std::chrono::steady_clock::time_point currentTime)
{
    const auto sendTime =
        getSendTime(message, currentTime, mWaitTime, mCurNodeId, mOwnNetworkSize, mMaxNumSendersPerMsg);
    if (!sendTime.has_value())
    {
        return;
    }

    const std::scoped_lock lock{mMutex};
    mMessages.push_back(pipe_queue::TimestampedMessage{
        .message = std::move(message), .timeReceived = currentTime, .sendTime = sendTime});
    std::push_heap(std::begin(mMessages), std::end(mMessages), compareByResend);
}

/* This function returns the most stale unsent message that should be sent at the current time
 */
std::optional<scrooge::CrossChainMessage> PipeQueue::getReadyMessage(std::chrono::steady_clock::time_point currentTime)
{
    const std::scoped_lock lock{mMutex};
    if (mMessages.empty())
    {
        return std::nullopt;
    }

    if (mMessages.front().sendTime > currentTime)
    {
        return std::nullopt;
    }

    std::pop_heap(std::begin(mMessages), std::end(mMessages), compareByResend);
    const auto readyMessage = std::move(mMessages.back().message);
    mMessages.pop_back();

    return readyMessage;
}

std::optional<std::chrono::steady_clock::time_point> PipeQueue::getNextReadyMessageTime() const
{
    const std::scoped_lock lock{mMutex};
    if (mMessages.empty())
    {
        return std::nullopt;
    }

    return mMessages.front().sendTime;
}
