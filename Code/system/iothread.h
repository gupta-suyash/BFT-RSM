#pragma once

#include "acknowledgment.h"
#include "acknowledgment_tracker.h"
#include "global.h"
#include "pipeline.h"
#include "quorum_acknowledgment.h"

#include <memory>

#include "readerwritercircularbuffer.h"

namespace iothread
{
using MessageQueue = moodycamel::BlockingReaderWriterCircularBuffer<scrooge::CrossChainMessage>;
}; // namespace iothread

void runGenerateMessageThread(std::shared_ptr<iothread::MessageQueue> messageOutput, NodeConfiguration configuration);

void runRelayIPCRequestThread(std::shared_ptr<iothread::MessageQueue> messageOutput);

void runSendThread(std::shared_ptr<iothread::MessageQueue> messageInput, std::shared_ptr<Pipeline> pipeline,
                   std::shared_ptr<Acknowledgment> acknowledgment, std::shared_ptr<AcknowledgmentTracker> ackTracker,
                   std::shared_ptr<QuorumAcknowledgment> quorumAck, NodeConfiguration configuration);

void runAllToAllSendThread(const std::shared_ptr<iothread::MessageQueue> messageInput,
                           const std::shared_ptr<Pipeline> pipeline,
                           const std::shared_ptr<Acknowledgment> acknowledgment,
                           const std::shared_ptr<AcknowledgmentTracker> ackTracker,
                           const std::shared_ptr<QuorumAcknowledgment> quorumAck,
                           const NodeConfiguration configuration);

void runOneToOneSendThread(const std::shared_ptr<iothread::MessageQueue> messageInput,
                           const std::shared_ptr<Pipeline> pipeline,
                           const std::shared_ptr<Acknowledgment> acknowledgment,
                           const std::shared_ptr<AcknowledgmentTracker> ackTracker,
                           const std::shared_ptr<QuorumAcknowledgment> quorumAck,
                           const NodeConfiguration configuration);

void runReceiveThread(std::shared_ptr<Pipeline> pipeline, std::shared_ptr<Acknowledgment> acknowledgment,
                      std::shared_ptr<AcknowledgmentTracker> ackTracker,
                      std::shared_ptr<QuorumAcknowledgment> quorumAck, NodeConfiguration configuration);

void naiveReceiveThread(const std::shared_ptr<Pipeline> pipeline, const std::shared_ptr<Acknowledgment> acknowledgment,
                        const std::shared_ptr<AcknowledgmentTracker> ackTracker,
                        const std::shared_ptr<QuorumAcknowledgment> quorumAck, const NodeConfiguration configuration);
