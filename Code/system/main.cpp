#include "acknowledgment.h"
#include "all_to_all.h"
#include "config.h"
#include "geobft.h"
#include "global.h"
#include "iothread.h"
#include "leader_leader.h"
#include "one_to_one.h"
#include "parser.h"
#include "pipeline.h"
#include "quorum_acknowledgment.h"
#include "scrooge.h"
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <thread>

int main(int argc, char *argv[])
{
    std::thread relayRequestThread;
    std::thread relayTransactionThread;
    std::string path;
    {
        const auto kCommandLineArguments = parseCommandLineArguments(argc, argv);
        const auto &[kOwnNetworkSize, kOtherNetworkSize, kOwnMaxNumFailedStake, kOtherMaxNumFailedStake, kNodeId,
                     kLogPath, kWorkingDir] = kCommandLineArguments;
        const auto nwPath = "/home/scrooge/"s;
        const auto kNetworkZeroConfigPath = nwPath + "network0urls.txt"s;
        const auto kNetworkOneConfigPath = nwPath + "network1urls.txt"s;

        const auto kOwnNetworkConfiguration =
            parseNetworkUrlsAndStake(get_rsm_id() ? kNetworkOneConfigPath : kNetworkZeroConfigPath);
        const auto kOtherNetworkConfiguration =
            parseNetworkUrlsAndStake(get_other_rsm_id() ? kNetworkOneConfigPath : kNetworkZeroConfigPath);

        const auto kNodeConfiguration =
            createNodeConfiguration(kCommandLineArguments, kOwnNetworkConfiguration, kOtherNetworkConfiguration);

        SPDLOG_CRITICAL(
            "Config set: kNumLocalNodes = {}, kNumForeignNodes = {}, kMaxNumLocalFailedNodes = {}, "
            "kMaxNumForeignFailedNodes = {}, kOwnNodeId = {}, g_rsm_id = {}, num_packets = {},  packet_size = {}, "
            "kLogPath= '{}'",
            kOwnNetworkSize, kOtherNetworkSize, kOwnMaxNumFailedStake, kOtherMaxNumFailedStake, kNodeId, get_rsm_id(),
            get_number_of_packets(), get_packet_size(), kLogPath);
        printMetrics(kLogPath); // Deletes the metrics!

        const auto kQuorumSize = kNodeConfiguration.kOtherMaxNumFailedStake + 1;
        constexpr auto kMessageBufferSize = MESSAGE_BUFFER_SIZE; // Old value: 256

        const auto acknowledgment = std::make_shared<Acknowledgment>();
        const auto pipeline = std::make_shared<Pipeline>(kOwnNetworkConfiguration.kNetworkUrls,
                                                         kOtherNetworkConfiguration.kNetworkUrls, kNodeConfiguration);
        const auto messageBuffer =
            std::make_shared<iothread::MessageQueue<scrooge::CrossChainMessageData>>(kMessageBufferSize);
        const auto quorumAck = std::make_shared<QuorumAcknowledgment>(kQuorumSize);
        const auto resendDataQueue =
            std::make_shared<iothread::MessageQueue<acknowledgment_tracker::ResendData>>(32768 * 2);
        const auto receivedMessageQueue = std::make_shared<iothread::MessageQueue<scrooge::CrossChainMessage>>(1 << 15);

        pipeline->startPipeline();

        const auto kTestStartTime = std::chrono::steady_clock::now();
        const auto testStartRecordTime = kTestStartTime + get_test_warmup_duration();
        const auto testEndTime = testStartRecordTime + get_test_duration();
        set_test_start(kTestStartTime);

        SPDLOG_INFO("Done setting up sockets between nodes.");

        set_priv_key();

        if constexpr (SIMULATE_CRASH)
        {
            SPDLOG_CRITICAL("Running with crashes simulated");
            const bool kIsCrashed = (kNodeId % 3) == 2;
            if (kIsCrashed)
            {
                SPDLOG_CRITICAL("Node {} in RSM {} Is Crashed", kNodeId, get_rsm_id());
                auto receiveThread = std::thread(runCrashedNodeReceiveThread, pipeline);

                std::this_thread::sleep_until(testEndTime);
                end_test();

                receiveThread.join();
                SPDLOG_CRITICAL("Crashed Node {} in RSM {} Finished Test", kNodeId, get_rsm_id());
                remove(kLogPath.c_str());
                return 0;
            }
        }

#if SCROOGE
#if FILE_RSM
        auto sendThread = std::thread(runFileScroogeSendThread, messageBuffer, pipeline, acknowledgment,
                                      resendDataQueue, quorumAck, kNodeConfiguration);
#else
        auto sendThread = std::thread(runScroogeSendThread, messageBuffer, pipeline, acknowledgment, resendDataQueue,
                                      quorumAck, kNodeConfiguration);
        auto relayRequestThread = std::thread(runRelayIPCRequestThread, messageBuffer, kNodeConfiguration);
        auto relayTransactionThread = std::thread(runRelayIPCTransactionThread, "/tmp/scrooge-output", quorumAck,
                                                  kNodeConfiguration, receivedMessageQueue);
        SPDLOG_INFO("Created Generate message relay thread");

#endif

        auto receiveThread = std::thread(runScroogeReceiveThread, pipeline, acknowledgment, resendDataQueue, quorumAck,
                                         kNodeConfiguration, receivedMessageQueue);
#elif ALL_TO_ALL
#if FILE_RSM
        auto sendThread = std::thread(runFileAllToAllSendThread, messageBuffer, pipeline, acknowledgment,
                                      resendDataQueue, quorumAck, kNodeConfiguration);
#else
        auto sendThread = std::thread(runAllToAllSendThread, messageBuffer, pipeline, acknowledgment, resendDataQueue,
                                      quorumAck, kNodeConfiguration);
        auto relayRequestThread = std::thread(runRelayIPCRequestThread, messageBuffer, kNodeConfiguration);
        auto relayTransactionThread = std::thread(runRelayIPCTransactionThread, "/tmp/scrooge-output", quorumAck,
                                                  kNodeConfiguration, receivedMessageQueue);
        SPDLOG_INFO("Created Generate message relay thread");

#endif

        auto receiveThread = std::thread(runAllToAllReceiveThread, pipeline, acknowledgment, resendDataQueue, quorumAck,
                                         kNodeConfiguration, receivedMessageQueue);
#elif GEOBFT
#if FILE_RSM
        auto sendThread = std::thread(runFileGeoBFTSendThread, messageBuffer, pipeline, acknowledgment, resendDataQueue,
                                      quorumAck, kNodeConfiguration);
#else
        auto sendThread = std::thread(runGeoBFTSendThread, messageBuffer, pipeline, acknowledgment, resendDataQueue,
                                      quorumAck, kNodeConfiguration);
        relayRequestThread = std::thread(runRelayIPCRequestThread, messageBuffer, kNodeConfiguration);
        relayTransactionThread = std::thread(runRelayIPCTransactionThread, "/tmp/scrooge-output", quorumAck,
                                             kNodeConfiguration, receivedMessageQueue);
        SPDLOG_INFO("GeoBFT: Created Generate message relay thread");
#endif
        auto receiveThread = std::thread(runGeoBFTReceiveThread, pipeline, acknowledgment, resendDataQueue, quorumAck,
                                         kNodeConfiguration, receivedMessageQueue);
#elif LEADER
#if FILE_RSM
        auto sendThread = std::thread(runFileLeaderSendThread, messageBuffer, pipeline, acknowledgment, resendDataQueue,
                                      quorumAck, kNodeConfiguration);
#else
        auto sendThread = std::thread(runLeaderSendThread, messageBuffer, pipeline, acknowledgment, resendDataQueue,
                                      quorumAck, kNodeConfiguration);
        relayRequestThread = std::thread(runRelayIPCRequestThread, messageBuffer, kNodeConfiguration);
        relayTransactionThread = std::thread(runRelayIPCTransactionThread, "/tmp/scrooge-output", quorumAck,
                                             kNodeConfiguration, receivedMessageQueue);
        SPDLOG_INFO("Leader: Created Generate message relay thread");
#endif
        auto receiveThread = std::thread(runLeaderReceiveThread, pipeline, acknowledgment, resendDataQueue, quorumAck,
                                         kNodeConfiguration, receivedMessageQueue);

#else
#if FILE_RSM
        auto sendThread = std::thread(runFileUnfairOneToOneSendThread, messageBuffer, pipeline, acknowledgment,
                                      resendDataQueue, quorumAck, kNodeConfiguration);
#else
        auto sendThread = std::thread(runUnfairOneToOneSendThread, messageBuffer, pipeline, acknowledgment,
                                      resendDataQueue, quorumAck, kNodeConfiguration);
        auto relayRequestThread = std::thread(runRelayIPCRequestThread, messageBuffer, kNodeConfiguration);
        auto relayTransactionThread = std::thread(runRelayIPCTransactionThread, "/tmp/scrooge-output", quorumAck,
                                                  kNodeConfiguration, receivedMessageQueue);
        SPDLOG_INFO("Created Generate message relay thread");

#endif

        auto receiveThread = std::thread(runUnfairOneToOneReceiveThread, pipeline, acknowledgment, resendDataQueue,
                                         quorumAck, kNodeConfiguration, receivedMessageQueue);
#endif

        SPDLOG_INFO("Created Receiver Thread with ID={} ");

        std::this_thread::sleep_until(testStartRecordTime);
        const auto trueTestStartTime = std::chrono::steady_clock::now();
        start_recording();
        addMetric("starting_quack", quorumAck->getCurrentQuack().value_or(0));
        addMetric("starting_ack", acknowledgment->getAckIterator().value_or(0));

        std::this_thread::sleep_until(testEndTime);
        const auto trueTestEndTime = std::chrono::steady_clock::now();
        end_test();

        sendThread.join();
        receiveThread.join();
        SPDLOG_CRITICAL(
            "SCROOGE COMPLETE. For node with config: kNumLocalNodes = {}, kNumForeignNodes = {}, "
            "kMaxNumLocalFailedNodes = {}, "
            "kMaxNumForeignFailedNodes = {}, kOwnNodeId = {}, g_rsm_id = {}, packet_size = {},  kLogPath= '{}'",
            kOwnNetworkSize, kOtherNetworkSize, kOwnMaxNumFailedStake, kOtherMaxNumFailedStake, kNodeId, get_rsm_id(),
            get_packet_size(), kLogPath);

        const auto trueTestWarmupDuration = trueTestStartTime - kTestStartTime;
        const auto trueTestDuration = trueTestEndTime - trueTestStartTime;
        addMetric("message_size", get_packet_size());
        addMetric("duration_seconds", std::chrono::duration<double>{trueTestDuration}.count()); // legacy for eval.py
        addMetric("Experiment Time", std::chrono::duration<double>{trueTestDuration}.count());
        addMetric("Warmup Time", std::chrono::duration<double>{trueTestWarmupDuration}.count());
        addMetric("local_network_size", kOwnNetworkSize);
        addMetric("foreign_network_size", kOtherNetworkSize);
        addMetric("local_max_failed_stake", kOwnMaxNumFailedStake);
        addMetric("foreign_max_failed_stake", kOtherMaxNumFailedStake);
        addMetric("foreign_stake_total", kOtherMaxNumFailedStake);
        addMetric("local_stake", kNodeConfiguration.kOwnNetworkStakes.at(kNodeId));
        addMetric("kList_size", kListSize);
        path = kLogPath;

        printMetrics(path); // print before attempting to deconstruct everything -- get as many results as fesible, the
                            // test is over and send/recv threads exited

#if !FILE_RSM
        relayTransactionThread.join();
        relayRequestThread.join();
#endif
    }
    printMetrics(path); // print full metrics after deconstructing everything
    return 0;
}
