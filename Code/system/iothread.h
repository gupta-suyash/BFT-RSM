#pragma once

#include "global.h"

#include "acknowledgment.h"
#include "acknowledgment_tracker.h"
#include "message_scheduler.h"
#include "pipeline.h"
#include "quorum_acknowledgment.h"

#include <memory>

#include "readerwritercircularbuffer.h"

namespace iothread
{
using MessageQueue = moodycamel::BlockingReaderWriterCircularBuffer<scrooge::CrossChainMessageData>;

struct MessageResendData
{
    scrooge::CrossChainMessageData messageData;
    uint64_t firstDestinationResendNumber{};
    uint64_t numDestinationsSent{};
    message_scheduler::CompactDestinationList destinations{};
};
}; // namespace iothread

void runGenerateMessageThread(std::shared_ptr<iothread::MessageQueue> messageOutput, NodeConfiguration configuration);

void runRelayIPCRequestThread(std::shared_ptr<iothread::MessageQueue> messageOutput,
                              NodeConfiguration kNodeConfiguration);

void runRelayIPCTransactionThread(std::string scroogeOutputPipePath, std::shared_ptr<QuorumAcknowledgment> quorumAck,
                                  NodeConfiguration kNodeConfiguration);

void runSendThread(std::shared_ptr<iothread::MessageQueue> messageInput, std::shared_ptr<Pipeline> pipeline,
                   std::shared_ptr<Acknowledgment> acknowledgment, std::shared_ptr<AcknowledgmentTracker> ackTracker,
                   std::shared_ptr<QuorumAcknowledgment> quorumAck, NodeConfiguration configuration);

void runAllToAllSendThread(std::shared_ptr<iothread::MessageQueue> messageInput, std::shared_ptr<Pipeline> pipeline,
                           std::shared_ptr<Acknowledgment> acknowledgment,
                           std::shared_ptr<AcknowledgmentTracker> ackTracker,
                           std::shared_ptr<QuorumAcknowledgment> quorumAck, NodeConfiguration configuration);

void runOneToOneSendThread(std::shared_ptr<iothread::MessageQueue> messageInput, std::shared_ptr<Pipeline> pipeline,
                           std::shared_ptr<Acknowledgment> acknowledgment,
                           std::shared_ptr<AcknowledgmentTracker> ackTracker,
                           std::shared_ptr<QuorumAcknowledgment> quorumAck, NodeConfiguration configuration);

void runReceiveThread(std::shared_ptr<Pipeline> pipeline, std::shared_ptr<Acknowledgment> acknowledgment,
                      std::shared_ptr<AcknowledgmentTracker> ackTracker,
                      std::shared_ptr<QuorumAcknowledgment> quorumAck, NodeConfiguration configuration);

void runAllToAllReceiveThread(const std::shared_ptr<Pipeline> pipeline,
                              const std::shared_ptr<Acknowledgment> acknowledgment,
                              const std::shared_ptr<AcknowledgmentTracker> ackTracker,
                              const std::shared_ptr<QuorumAcknowledgment> quorumAck,
                              const NodeConfiguration configuration);

void runGenerateMessageThreadWithIpc();
