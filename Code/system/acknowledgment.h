#pragma once

#include "global.h"
#include <atomic>
#include <mutex>
#include <vector>

class Acknowledgment
{
  public:
    static constexpr uint64_t kMinimumAckValue = 0;

    void addToAckList(uint64_t ack);
    std::optional<uint64_t> getAckIterator() const;

  private:
    static constexpr uint64_t kWindowSize = 2ULL * (1ULL<<30);

    mutable std::mutex mMutex{};
    std::atomic<std::optional<uint64_t>> mAckValue{std::nullopt};
    std::vector<bool> mAckWindow = std::vector<bool>(kWindowSize);
};
