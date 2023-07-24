#include "proto_utils.h"

#include "crypto.h"

namespace util
{
bool testAckView(const JankAckView &ackView, const uint64_t ack)
{
    const auto kViewSize = ackView.view.size() * 64;
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

uint64_t getFinalAck(const JankAckView &ackView)
{
    for (int64_t i = ackView.view.size() - 1; i >= 0; i--)
    {
        const auto numRighZeros = std::countl_zero(ackView.view[i]);
        if (numRighZeros != 64)
        {
            return ackView.ackOffset + (i * 64) + (64 - numRighZeros) - 1;
        }
    }

    return ackView.ackOffset - 1;
}

std::optional<uint64_t> getAckIterator(const JankAckView &ackView)
{
    const auto ackIterator = (ackView.ackOffset > 1) ? std::optional<uint64_t>(ackView.ackOffset - 2) : std::nullopt;
    return ackIterator;
}

bool checkMessageMac(const scrooge::CrossChainMessage &message)
{
    // Verification TODO
    const auto mstr = message.ack_count().SerializeAsString();
    // Fetch the sender key
    // const auto senderKey = get_other_rsm_key(nng_message.senderId);
    const auto senderKey = get_priv_key();
    // Verify the message
    return CmacVerifyString(senderKey, mstr, message.validity_proof());
}

bool isMessageDataValid(const scrooge::CrossChainMessageData &message)
{
    // no signature checking currently
    return true;
}

scrooge::CrossChainMessageData getNextMessage()
{
    static uint64_t curSN{};
    scrooge::CrossChainMessageData msg;
    msg.set_sequence_number(curSN++);
    return msg;
}
}; // namespace util
