#pragma once

#include "global.h"

#include "acknowledgment.h"
#include "acknowledgment_tracker.h"
#include "message_scheduler.h"
#include "pipeline.h"
#include "quorum_acknowledgment.h"

#include <memory>

#include "readerwriterqueue.h"

namespace iothread
{
template <typename T> using MessageQueue = moodycamel::ReaderWriterQueue<T>;
}; // namespace iothread

void runRelayIPCRequestThread(std::shared_ptr<iothread::MessageQueue<scrooge::CrossChainMessageData>> messageOutput,
                              NodeConfiguration kNodeConfiguration);

void runRelayIPCTransactionThread(std::string scroogeOutputPipePath, std::shared_ptr<QuorumAcknowledgment> quorumAck,
                                  NodeConfiguration kNodeConfiguration);

void runGenerateMessageThreadWithIpc();

void runCrashedNodeReceiveThread(std::shared_ptr<Pipeline> pipeline);