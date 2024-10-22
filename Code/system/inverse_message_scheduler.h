#pragma once
#include "global.h"

#include "message_scheduler.h"

class InverseMessageScheduler
{
  public:
    InverseMessageScheduler(NodeConfiguration configuration);
    std::optional<uint64_t> getMinResendNumber(uint64_t sequenceNumber) const;

  private:
    uint64_t kResendNumberLookupBitMask{};
    std::vector<std::optional<uint64_t>> mResendNumberLookup{};
};
