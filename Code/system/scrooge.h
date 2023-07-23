#pragma once

#include "acknowledgment.h"
#include "acknowledgment_tracker.h"
#include "global.h"
#include "iothread.h"
#include "message_scheduler.h"
#include "pipeline.h"
#include "quorum_acknowledgment.h"

#include <memory>

namespace scrooge
{
struct MessageResendData
{
    uint64_t sequenceNumber{};
    uint64_t firstDestinationResendNumber{};
    uint64_t numDestinationsSent{};
    scrooge::CrossChainMessageData messageData;
    message_scheduler::CompactDestinationList destinations{};
};
}; // namespace scrooge

void runScroogeSendThread(std::shared_ptr<iothread::MessageQueue<scrooge::CrossChainMessageData>> messageInput,
                          std::shared_ptr<Pipeline> pipeline, std::shared_ptr<Acknowledgment> acknowledgment,
                          std::shared_ptr<iothread::MessageQueue<acknowledgment_tracker::ResendData>> resendDataQueue,
                          std::shared_ptr<QuorumAcknowledgment> quorumAck, NodeConfiguration configuration);

void runScroogeReceiveThread(
    std::shared_ptr<Pipeline> pipeline, std::shared_ptr<Acknowledgment> acknowledgment,
    std::shared_ptr<iothread::MessageQueue<acknowledgment_tracker::ResendData>> resendDataQueue,
    std::shared_ptr<QuorumAcknowledgment> quorumAck, NodeConfiguration configuration);
