#pragma once

#include "global.h"
#include <atomic>
#include <bit>
#include <vector>
#include <mutex>
#include <iostream>

namespace acknowledgment
{
  template<uint64_t kViewSize>
  struct AckView
  {
    static_assert(kViewSize % 64 == 0, "View Size must be a multiple of 64");
    constexpr static uint64_t kNumInts = kViewSize / 64;
    uint64_t ackOffset{}; // node's ackValue.value_or(-1) + 2. ackValue.value_or(-1)+1 is always missing
    std::array<uint64_t, kNumInts> view{};
  };

  template<uint64_t kViewSize>
  bool testAckView(const AckView<kViewSize>& ackView, const uint64_t ack)
  {
    if (ack == ackView.ackOffset - 1 || ack >= ackView.ackOffset + kViewSize)
    {
      return false;
    }
    if (ack < ackView.ackOffset)
    {
      return true;
    }
    const auto index = (ack - ackView.ackOffset) % kViewSize;
    return ackView.view[index / 64] & (1ULL << (index % 64));
  }
  template<uint64_t kViewSize>
  uint64_t getFinalAck(const AckView<kViewSize>& ackView)
  {
    if constexpr (kViewSize != 0)
    {
      for (int64_t i = ackView.view.size() - 1; i >= 0; i--)
      {
        const auto numRighZeros = std::countl_zero(ackView.view[i]);
        if (numRighZeros != 64)
        {
          return ackView.ackOffset + (i * 64) + (64 - numRighZeros) - 1;
        }
      }
    }

    return ackView.ackOffset - 1;
  }
  template<uint64_t kViewSize>
  std::optional<uint64_t> getAckIterator(const AckView<kViewSize>& ackView)
  {
    const auto ackIterator = (ackView.ackOffset > 1)
                             ? std::optional<uint64_t>(ackView.ackOffset - 2)
                             : std::nullopt;
    return ackIterator;
  }
}; // namespace acknowledgment

class Acknowledgment
{
  public:
    void addToAckList(uint64_t ack);
    std::optional<uint64_t> getAckIterator() const;
    
    template<uint64_t kViewSize>
    acknowledgment::AckView<kViewSize> getAckView() const
    {
      // std::scoped_lock lock{mMutex};
      acknowledgment::AckView<kViewSize> ackView{};
      const auto ackOffset = mAckValue.load(std::memory_order_acquire).value_or(0ULL - 1) + 2;
      ackView.ackOffset = ackOffset;

      const uint64_t initialAckValue = ackOffset % kWindowSize;
      const uint64_t *const mAckWindowData = (uint64_t*) mAckWindow.data();
      constexpr uint64_t wordSize = sizeof(uint64_t) * 8;
      constexpr uint64_t wordSizeBytes = sizeof(uint64_t);
      const uint64_t numWords = kViewSize / wordSize;
      const uint64_t initialAckWord = initialAckValue / wordSize;
      const uint64_t wordTearSize = initialAckValue % wordSize;
      const uint64_t isViewWrapped = kWindowSize - initialAckValue < kViewSize;

      if (isViewWrapped)
      {
          const auto wordsFromBack = mAckWindow.size() - initialAckWord;
          const auto wordsFromFront = numWords - wordsFromBack;
          std::memcpy(ackView.view.data(), mAckWindowData + initialAckWord, wordsFromBack * wordSizeBytes);
          std::memcpy(ackView.view.data(), mAckWindowData, wordsFromFront * wordSizeBytes);
          if (wordTearSize)
          {
              ackView.view.front() >>= wordTearSize;
              for (int i = 1; i < ackView.view.size(); i++)
              {
                  const auto discarded = ackView.view[i] << (wordSize - wordTearSize);
                  ackView.view[i - 1] |= discarded;
                  ackView.view[i] >>= wordTearSize;
              }
              const auto tornWord = *(mAckWindowData + wordsFromFront);
              ackView.view.back() |= tornWord << (wordSize - wordTearSize);
          }
          return ackView;
      }

      std::memcpy(ackView.view.data(), mAckWindowData + initialAckWord, numWords * wordSizeBytes);
      if (wordTearSize)
      {
          ackView.view.front() >>= wordTearSize;
          for (int i = 1; i < ackView.view.size(); i++)
          {
              const auto discarded = ackView.view[i] << (wordSize - wordTearSize);
              ackView.view[i - 1] |= discarded;
              ackView.view[i] >>= wordTearSize;
          }
          const auto tornWord = *(mAckWindowData + initialAckWord + numWords);
          ackView.view.back() |= tornWord << (wordSize - wordTearSize);
      }

      return ackView;
    }

  private:
    static constexpr uint64_t kWindowSize = 2ULL * (1ULL<<30);
    static_assert(kWindowSize % 64 == 0, "kWindowSize Must be a multiple of 64 (word size)");

    // mutable std::mutex mMutex;

    std::atomic<std::optional<uint32_t>> mAckValue{std::nullopt};
    std::vector<uint64_t> mAckWindow = std::vector<uint64_t>(kWindowSize / 64);
};
