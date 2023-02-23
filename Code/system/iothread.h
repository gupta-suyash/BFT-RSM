#pragma once

#include "acknowledgment.h"
#include "acknowledgment_tracker.h"
#include "global.h"
#include "pipeline.h"
#include "quorum_acknowledgment.h"

#include <memory>

#include <boost/fiber/buffered_channel.hpp>

namespace iothread
{
using MessageQueue = boost::fibers::buffered_channel<scrooge::CrossChainMessage>;
}; // namespace iothread

void runGenerateMessageThread(std::shared_ptr<iothread::MessageQueue> messageOutput, NodeConfiguration configuration);

void runRelayIPCRequestThread(std::shared_ptr<iothread::MessageQueue> messageOutput);

void runSendThread(std::shared_ptr<iothread::MessageQueue> messageInput, std::shared_ptr<Pipeline> pipeline,
                   std::shared_ptr<Acknowledgment> acknowledgment, std::shared_ptr<AcknowledgmentTracker> ackTracker,
                   std::shared_ptr<QuorumAcknowledgment> quorumAck, NodeConfiguration configuration);

void runReceiveThread(std::shared_ptr<Pipeline> pipeline, std::shared_ptr<Acknowledgment> acknowledgment,
                      std::shared_ptr<AcknowledgmentTracker> ackTracker,
                      std::shared_ptr<QuorumAcknowledgment> quorumAck, NodeConfiguration configuration);
