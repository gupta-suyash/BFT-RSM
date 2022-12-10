#pragma once

#include "global.h"
#include "scrooge_message.pb.h"

#include <algorithm>
#include <chrono>
#include <mutex>
#include <vector>

namespace pipe_queue
{
struct TimestampedMessage
{
    scrooge::CrossChainMessage message;
    // the time this node first was made aware of this message
    std::chrono::steady_clock::time_point timeReceived;
    // The time that this node should send this message by if necessary
    // Nodes do not need to send messages the other network already acknowledges
    std::optional<std::chrono::steady_clock::time_point> sendTime;
};
}; // namespace pipe_queue

class PipeQueue
{
  public:
    PipeQueue(uint64_t curNodeId, uint64_t ownNetworkSize, uint64_t maxNumSendersPerMsg,
              std::chrono::steady_clock::time_point::duration waitTime);

    void addMessage(scrooge::CrossChainMessage &&msg, std::chrono::steady_clock::time_point currentTime);
    std::optional<scrooge::CrossChainMessage> getReadyMessage(std::chrono::steady_clock::time_point currentTime);

    std::optional<std::chrono::steady_clock::time_point> getNextReadyMessageTime() const;

  private:
    mutable std::mutex mMutex;

    const uint64_t mCurNodeId;
    const uint64_t mOwnNetworkSize;
    const uint64_t mMaxNumSendersPerMsg;
    const std::chrono::steady_clock::time_point::duration mWaitTime;

    std::vector<pipe_queue::TimestampedMessage> mMessages;
};
