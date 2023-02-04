#include "acknowledgment.h"
#include "global.h"
#include "iothread.h"
#include "parser.h"
#include "pipeline.h"
#include "quorum_acknowledgment.h"

#include <chrono>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>

int main(int argc, char *argv[])
{
    const auto curTime = std::chrono::steady_clock::now();
    const auto roundedTime = std::chrono::duration_cast<std::chrono::seconds>(curTime.time_since_epoch()) + 1s;
    const auto startTime = std::chrono::steady_clock::time_point(roundedTime);
    std::this_thread::sleep_until(startTime);

    // Parsing the command line args.
    const auto kNodeConfiguration = parser(argc, argv);
    const auto &[kOwnNetworkSize, kOtherNetworkSize, kOwnMaxNumFailedNodes, kOtherMaxNumFailedNodes, kNodeId] =
        kNodeConfiguration;
    const auto kWorkingDir = std::filesystem::current_path();
    const auto kNetworkZeroConfigPath = kWorkingDir / "configuration/network0urls.txt";
    const auto kNetworkOneConfigPath = kWorkingDir / "configuration/network1urls.txt";

    SPDLOG_INFO("Config set: kNumLocalNodes = {}, kNumForeignNodes = {}, kMaxNumLocalFailedNodes = {}, "
                "kMaxNumForeignFailedNodes = {}, kOwnNodeId = {}, g_rsm_id = {}",
                kOwnNetworkSize, kOtherNetworkSize, kOwnMaxNumFailedNodes, kOtherMaxNumFailedNodes, kNodeId, get_rsm_id());

    auto ownNetworkUrls = parseNetworkUrls(get_rsm_id() ? kNetworkOneConfigPath : kNetworkZeroConfigPath);
    auto otherNetworkUrls = parseNetworkUrls(get_other_rsm_id() ? kNetworkOneConfigPath : kNetworkZeroConfigPath);

    const auto kQuorumSize = kNodeConfiguration.kOtherMaxNumFailedNodes + 1;
    const auto kMessageBufferSize = 2048;
    SPDLOG_INFO("Before ack");
    const auto acknowledgment = std::make_shared<Acknowledgment>();
    const auto pipeline =
        std::make_shared<Pipeline>(std::move(ownNetworkUrls), std::move(otherNetworkUrls), kNodeConfiguration);
    const auto messageBuffer = std::make_shared<iothread::MessageQueue>(kMessageBufferSize);
    const auto ackTracker = std::make_shared<AcknowledgmentTracker>();
    const auto quorumAck = std::make_shared<QuorumAcknowledgment>(kQuorumSize);

    pipeline->startPipeline();
    SPDLOG_INFO("Done setting up sockets between nodes.");

    const auto kThreadHasher = std::hash<std::thread::id>{};
    auto messageRelayThread = std::thread(runGenerateMessageThread, messageBuffer, kNodeConfiguration);
    SPDLOG_INFO("Created Generate FAKE MESSAGE for testing thread with ID={}",
                kThreadHasher(messageRelayThread.get_id()));

    auto sendThread =
        std::thread(runSendThread, messageBuffer, pipeline, acknowledgment, ackTracker, quorumAck, kNodeConfiguration);
    SPDLOG_INFO("Created Sender Thread with ID={} ", kThreadHasher(sendThread.get_id()));

    auto receiveThread =
        std::thread(runReceiveThread, pipeline, acknowledgment, ackTracker, quorumAck, kNodeConfiguration);
    SPDLOG_INFO("Created Receiver Thread with ID={} ", kThreadHasher(receiveThread.get_id()));

    messageRelayThread.join();
    sendThread.join();
    receiveThread.join();

    return 0;
}
