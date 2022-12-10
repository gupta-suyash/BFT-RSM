#include "acknowledgment.h"
#include "global.h"
#include "iothread.h"
#include "parser.h"
#include "pipe_queue.h"
#include "pipeline.h"
#include "quorum_acknowledgment.h"

#include <functional>
#include <memory>
#include <string>
#include <thread>

int main(int argc, char *argv[])
{
    // Parsing the command line args.
    parser(argc, argv);

    const auto kCurNodeId = get_node_id();
    const auto kOwnNetworkSize = get_nodes_rsm();
    const auto kQuorumSize = get_max_nodes_fail(true) + 1;
    const auto kMaxNumMsgResenders = get_max_nodes_fail(true) + get_max_nodes_fail(false) + 1;
    const auto kResendWaitPeriod = 5s;

    const auto acknowledgment = std::make_shared<Acknowledgment>();
    const auto pipeline = std::make_shared<Pipeline>();
    const auto pipeQueue =
        std::make_shared<PipeQueue>(kCurNodeId, kOwnNetworkSize, kMaxNumMsgResenders, kResendWaitPeriod);
    const auto quorumAck = std::make_shared<QuorumAcknowledgment>(kQuorumSize);

    pipeline->SetSockets();
    SPDLOG_INFO("Done setting up sockets between nodes.");

    const auto kThreadHasher = std::hash<std::thread::id>{};
    auto messageRelayThread = std::thread(runGenerateMessageThread, pipeQueue);
    SPDLOG_INFO("Created Generate FAKE MESSAGE for testing thread with ID={}",
                kThreadHasher(messageRelayThread.get_id()));

    auto sendThread = std::thread(runSendThread, pipeQueue, pipeline, acknowledgment, quorumAck);
    SPDLOG_INFO("Created Sender Thread with ID={} ", kThreadHasher(sendThread.get_id()));

    auto receiveThread = std::thread(runReceiveThread, pipeline, acknowledgment, quorumAck);
    SPDLOG_INFO("Created Receiver Thread with ID={} ", kThreadHasher(receiveThread.get_id()));

    messageRelayThread.join();
    sendThread.join();
    receiveThread.join();

    return 0;
}
