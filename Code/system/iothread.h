#pragma once

#include "global.h"
#include "pipe_queue.h"
#include "pipeline.h"

#include <memory>

void runSendThread(std::shared_ptr<PipeQueue> pipeQueue, std::shared_ptr<Pipeline> pipeline);

void runReceiveThread(std::shared_ptr<Pipeline> pipeline);
