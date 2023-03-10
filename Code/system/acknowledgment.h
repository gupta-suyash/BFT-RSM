#pragma once

#include "global.h"
#include <atomic>

#include <boost/icl/interval_set.hpp>

class Acknowledgment
{
  public:
    static constexpr uint64_t kMinimumAckValue = 0;

    void addToAckList(uint64_t nodeId);
    std::optional<uint64_t> getAckIterator() const;

  private:
    std::atomic<std::optional<uint64_t>> mAckValue{std::nullopt};
    boost::icl::interval_set<uint64_t> mAckWindows;
};
