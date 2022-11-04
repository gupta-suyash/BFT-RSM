#pragma once

#include <boost/icl/interval_set.hpp>
#include <mutex>

#include "global.h"

class Acknowledgment
{
  public:
    void addToAckList(uint64_t nodeId);
    std::optional<uint64_t> getAckIterator() const;

  private:
    mutable std::mutex mMutex;

    std::optional<uint64_t> mAckValue{std::nullopt};
    boost::icl::interval_set<uint64_t> mAckWindows;
};
