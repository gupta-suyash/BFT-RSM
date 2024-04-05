#include "inverse_message_scheduler.h"

static uint64_t bitCeil(uint64_t value)
{
    value--;             // if value is already a power of 2
    value |= value >> 1; // paint highest bit everywhere
    value |= value >> 2;
    value |= value >> 4;
    value |= value >> 8;
    value |= value >> 16;
    value |= value >> 32;
    value++; // add one to find pow2 > value - 1
    return value;
}

InverseMessageScheduler::InverseMessageScheduler(const NodeConfiguration configuration)
{
    for (uint64_t curSenderId = 0; curSenderId < configuration.kOtherNetworkSize; curSenderId++)
    {
        const auto curSenderConfiguration =
            NodeConfiguration{.kOwnNetworkSize = configuration.kOtherNetworkSize,
                              .kOtherNetworkSize = configuration.kOwnNetworkSize,
                              .kOwnNetworkStakes = configuration.kOtherNetworkStakes,
                              .kOtherNetworkStakes = configuration.kOwnNetworkStakes,
                              .kOwnMaxNumFailedStake = configuration.kOtherMaxNumFailedStake,
                              .kOtherMaxNumFailedStake = configuration.kOwnMaxNumFailedStake,
                              .kNodeId = curSenderId,
                              .kLogPath = "",
                              .kWorkingDir = ""};
        const auto curSenderScheduler = MessageScheduler(curSenderConfiguration);
        const auto messageCycleLength = curSenderScheduler.getMessageCycleLength();

        mResendNumberLookup.resize(std::max(mResendNumberLookup.size(), messageCycleLength));

        for (uint64_t curMessage{}; curMessage < messageCycleLength; curMessage++)
        {
            const auto curMessageResendNum = curSenderScheduler.getResendNumber(curMessage);
            if (curMessageResendNum.has_value())
            {
                const auto initialInsert = not mResendNumberLookup.at(curMessage).has_value();
                const auto updateValue = mResendNumberLookup.at(curMessage) > curMessageResendNum;
                if (initialInsert || updateValue)
                {
                    mResendNumberLookup.at(curMessage) = curMessageResendNum;
                }
            }
        }
    }
    for (auto& resendNumber : mResendNumberLookup)
    {
        // If we are an original sender then don't worry about resending
        if (resendNumber.value() == 0)
        {
            // resendNumber.reset();
        }
    }
    assert("ResendNumberLookup must be a power of 2" &&
           bitCeil(mResendNumberLookup.size()) == mResendNumberLookup.size());
    kResendNumberLookupBitMask = mResendNumberLookup.size() - 1;
}

std::optional<uint64_t> InverseMessageScheduler::getMinResendNumber(uint64_t sequenceNumber) const
{
    return mResendNumberLookup[sequenceNumber % kResendNumberLookupBitMask];
}