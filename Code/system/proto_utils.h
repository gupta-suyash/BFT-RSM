#pragma once

#include "global.h"
#include "scrooge_message.pb.h"

namespace util
{
struct JankAckView
{
    uint32_t senderId;
    uint32_t ackOffset;
    google::protobuf::RepeatedField<uint64_t> view;
};

bool testAckView(const JankAckView &ackView, const uint64_t ack);

uint64_t getFinalAck(const JankAckView &ackView);

std::optional<uint64_t> getAckIterator(const JankAckView &ackView);

bool checkMessageMac(const scrooge::CrossChainMessage &message);

bool isMessageDataValid(const scrooge::CrossChainMessageData &message);

scrooge::CrossChainMessageData getNextMessage();
}; // namespace util
