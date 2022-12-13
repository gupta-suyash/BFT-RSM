#include "acknowledgment.h"
#include "global.h"
#include "iothread.h"
#include "parser.h"
#include "pipeline.h"
#include "quorum_acknowledgment.h"

#include <functional>
#include <memory>
#include <string>
#include <thread>

int main(int argc, char *argv[])
{
    // Parsing the command line args.
    const auto nodeConfiguration = parser(argc, argv);

    const auto kQuorumSize = nodeConfiguration.kOtherNetworkSize + 1;
    const auto kMessageBufferSize = 100;

    const auto acknowledgment = std::make_shared<Acknowledgment>();
    const auto pipeline = std::make_shared<Pipeline>();
    const auto messageBuffer = std::make_shared<iothread::MessageQueue>(kMessageBufferSize);
    const auto ackTracker = std::make_shared<AcknowledgmentTracker>();
    const auto quorumAck = std::make_shared<QuorumAcknowledgment>(kQuorumSize);

    pipeline->SetSockets();
    SPDLOG_INFO("Done setting up sockets between nodes.");

    const auto kThreadHasher = std::hash<std::thread::id>{};
    auto messageRelayThread = std::thread(runGenerateMessageThread, messageBuffer);
    SPDLOG_INFO("Created Generate FAKE MESSAGE for testing thread with ID={}",
                kThreadHasher(messageRelayThread.get_id()));

    auto sendThread = std::thread(runSendThread, messageBuffer, pipeline, acknowledgment, ackTracker, quorumAck, nodeConfiguration);
    SPDLOG_INFO("Created Sender Thread with ID={} ", kThreadHasher(sendThread.get_id()));

    auto receiveThread = std::thread(runReceiveThread, pipeline, acknowledgment, ackTracker, quorumAck, nodeConfiguration);
    SPDLOG_INFO("Created Receiver Thread with ID={} ", kThreadHasher(receiveThread.get_id()));

    messageRelayThread.join();
    sendThread.join();
    receiveThread.join();

    return 0;
}
