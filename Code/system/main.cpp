#include "acknowledgment.h"
#include "global.h"
#include "iothread.h"
#include "parser.h"
#include "pipeline.h"
#include "quorum_acknowledgment.h"
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <thread>

int main(int argc, char *argv[])
{
    const auto kCommandLineArguments = parseCommandLineArguments(argc, argv);
    const auto &[kOwnNetworkSize, kOtherNetworkSize, kOwnMaxNumFailedStake, kOtherMaxNumFailedStake, kNodeId, kLogPath,
                 kWorkingDir] = kCommandLineArguments;
    const auto kNetworkZeroConfigPath = kWorkingDir + "network0urls.txt"s;
    const auto kNetworkOneConfigPath = kWorkingDir + "network1urls.txt"s;

    const auto kOwnNetworkConfiguration =
        parseNetworkUrlsAndStake(get_rsm_id() ? kNetworkOneConfigPath : kNetworkZeroConfigPath);
    const auto kOtherNetworkConfiguration =
        parseNetworkUrlsAndStake(get_other_rsm_id() ? kNetworkOneConfigPath : kNetworkZeroConfigPath);

    const auto kNodeConfiguration =
        createNodeConfiguration(kCommandLineArguments, kOwnNetworkConfiguration, kOtherNetworkConfiguration);

    SPDLOG_INFO("Config set: kNumLocalNodes = {}, kNumForeignNodes = {}, kMaxNumLocalFailedNodes = {}, "
                "kMaxNumForeignFailedNodes = {}, kOwnNodeId = {}, g_rsm_id = {}, num_packets = {},  packet_size = {}, "
                "kLogPath= '{}'",
                kOwnNetworkSize, kOtherNetworkSize, kOwnMaxNumFailedStake, kOtherMaxNumFailedStake, kNodeId,
                get_rsm_id(), get_number_of_packets(), get_packet_size(), kLogPath);

    const auto kQuorumSize = kNodeConfiguration.kOtherMaxNumFailedStake + 1;
    constexpr auto kMessageBufferSize = 8192;

    const auto acknowledgment = std::make_shared<Acknowledgment>();
    const auto pipeline =
        std::make_shared<Pipeline>(std::move(kOwnNetworkConfiguration.kNetworkUrls),
                                   std::move(kOwnNetworkConfiguration.kNetworkUrls), kNodeConfiguration);
    const auto messageBuffer = std::make_shared<iothread::MessageQueue>(kMessageBufferSize);
    const auto ackTracker = std::make_shared<AcknowledgmentTracker>(kNodeConfiguration.kOtherNetworkSize,
                                                                    kNodeConfiguration.kOtherMaxNumFailedStake);
    const auto quorumAck = std::make_shared<QuorumAcknowledgment>(kQuorumSize);

    set_test_start(std::chrono::steady_clock::now());

    pipeline->startPipeline();
    SPDLOG_INFO("Done setting up sockets between nodes.");

    set_priv_key();
    // std::cout << "Done setting private key." << endl;

    // KeyExchange tasks TODO
    // constexpr auto kSleepTime = 2s;
    // pipeline->BroadcastKeyToOwnRsm();
    // std::this_thread::sleep_for(kSleepTime);
    // pipeline->RecvFromOwnRsm();
    // std::this_thread::sleep_for(kSleepTime);

    const auto kThreadHasher = std::hash<std::thread::id>{};
    auto messageRelayThread = std::thread(runGenerateMessageThread, messageBuffer, kNodeConfiguration);
    // auto relayRequestThread = std::thread(runRelayIPCRequestThread, messageBuffer);
    // auto relayTransactionThread = std::thread(runRelayIPCTransactionThread, "/tmp/scrooge-output"s, quorumAck);
    SPDLOG_INFO("Created Generate message relay thread ID={}", kThreadHasher(messageRelayThread.get_id()));

    auto sendThread =
        std::thread(runSendThread, messageBuffer, pipeline, acknowledgment, ackTracker, quorumAck, kNodeConfiguration);
    auto receiveThread =
        std::thread(runReceiveThread, pipeline, acknowledgment, ackTracker, quorumAck, kNodeConfiguration);
    SPDLOG_INFO("Created Receiver Thread with ID={} ", kThreadHasher(receiveThread.get_id()));

    messageRelayThread.join();
    sendThread.join();
    receiveThread.join();
    // relayRequestThread.join()
    // relayTransactionThread.join();

    SPDLOG_CRITICAL("SCROOGE COMPLETE. For node with config: kNumLocalNodes = {}, kNumForeignNodes = {}, "
                    "kMaxNumLocalFailedNodes = {}, "
                    "kMaxNumForeignFailedNodes = {}, kOwnNodeId = {}, g_rsm_id = {}, packet_size = {},  kLogPath= '{}'",
                    kOwnNetworkSize, kOtherNetworkSize, kOwnMaxNumFailedStake, kOtherMaxNumFailedStake, kNodeId,
                    get_rsm_id(), get_packet_size(), kLogPath);

    
    addMetric("message_size", get_packet_size());
    addMetric("duration_seconds", std::chrono::duration<double>{get_test_duration()}.count());
    addMetric("transfer_strategy", "2Send2Recv v2 Thread scrooge");
    printMetrics(kLogPath);
    return 0;
}
