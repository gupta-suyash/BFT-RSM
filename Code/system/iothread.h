#pragma once

#include "acknowledgment.h"
#include "acknowledgment_tracker.h"
#include "global.h"
#include "pipeline.h"
#include "quorum_acknowledgment.h"

#include <memory>

#include <boost/lockfree/spsc_queue.hpp>

namespace iothread {
using MessageQueue = boost::lockfree::spsc_queue<scrooge::CrossChainMessage>;
}; // namespace iothread

void runGenerateMessageThread(std::shared_ptr<iothread::MessageQueue> messageOutput);

void runRelayIPCMessageThread(std::shared_ptr<iothread::MessageQueue> messageOutput);

void runSendThread(std::shared_ptr<iothread::MessageQueue> messageInput,
                   std::shared_ptr<Pipeline> pipeline, std::shared_ptr<Acknowledgment> acknowledgment,
                   std::shared_ptr<AcknowledgmentTracker> ackTracker, std::shared_ptr<QuorumAcknowledgment> quorumAck, NodeConfiguration configuration);

void runReceiveThread(std::shared_ptr<Pipeline> pipeline, std::shared_ptr<Acknowledgment> acknowledgment,
                      std::shared_ptr<AcknowledgmentTracker> ackTracker, std::shared_ptr<QuorumAcknowledgment> quorumAck, NodeConfiguration configuration);
