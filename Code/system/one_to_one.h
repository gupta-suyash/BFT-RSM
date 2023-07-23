#pragma once

#include "acknowledgment.h"
#include "acknowledgment_tracker.h"
#include "global.h"
#include "iothread.h"
#include "message_scheduler.h"
#include "pipeline.h"
#include "quorum_acknowledgment.h"

#include <memory>

void runOneToOneSendThread(std::shared_ptr<iothread::MessageQueue<scrooge::CrossChainMessageData>> messageInput,
                           std::shared_ptr<Pipeline> pipeline, std::shared_ptr<Acknowledgment> acknowledgment,
                           std::shared_ptr<iothread::MessageQueue<acknowledgment_tracker::ResendData>> resendDataQueue,
                           std::shared_ptr<QuorumAcknowledgment> quorumAck, NodeConfiguration configuration);

void runUnfairOneToOneSendThread(std::shared_ptr<iothread::MessageQueue<scrooge::CrossChainMessageData>> messageInput,
                                 std::shared_ptr<Pipeline> pipeline, std::shared_ptr<Acknowledgment> acknowledgment,
                                 std::shared_ptr<std::vector<std::unique_ptr<AcknowledgmentTracker>>> ackTrackers,
                                 std::shared_ptr<QuorumAcknowledgment> quorumAck, NodeConfiguration configuration);

void runOneToOneReceiveThread(
    std::shared_ptr<Pipeline> pipeline, std::shared_ptr<Acknowledgment> acknowledgment,
    std::shared_ptr<iothread::MessageQueue<acknowledgment_tracker::ResendData>> resendDataQueue,
    std::shared_ptr<QuorumAcknowledgment> quorumAck, NodeConfiguration configuration);