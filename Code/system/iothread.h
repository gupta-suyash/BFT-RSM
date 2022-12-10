#pragma once

#include "acknowledgment.h"
#include "global.h"
#include "pipe_queue.h"
#include "pipeline.h"
#include "quorum_acknowledgment.h"

#include <memory>

void runGenerateMessageThread(std::shared_ptr<PipeQueue> pipeQueue);

void runRelayIPCMessageThread(std::shared_ptr<PipeQueue> pipeQueue);

void runSendThread(std::shared_ptr<PipeQueue> pipeQueue, std::shared_ptr<Pipeline> pipeline,
                   std::shared_ptr<Acknowledgment> acknowledgment, std::shared_ptr<QuorumAcknowledgment> quorumAck);

void runReceiveThread(std::shared_ptr<Pipeline> pipeline, std::shared_ptr<Acknowledgment> acknowledgment,
                      std::shared_ptr<QuorumAcknowledgment> quorumAck);
