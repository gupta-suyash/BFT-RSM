#include "acknowledgment.h"
#include "global.h"
#include "iothread.h"
#include "parser.h"
#include "pipeline.h"
#include "quorum_acknowledgment.h"
#include "benchmark.h"
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <thread>


int main(int argc, char *argv[])
{
    // if(argc < 3) {
    //     std::cout << "2 args plz" << std::endl;
    //     return -1;
    // }
    // std::size_t pos;
    // uint64_t max_power = std::stoi(argv[1], &pos);
    // std::size_t posTwo;
    // uint64_t message_sz = std::stoi(argv[1], &pos);
    // test_integer(max_power);
    // test_protobuf_creation_stack(100);
    // test_protobuf_creation_heap(100);
    // std::cout << "Hi " << std::endl;
    // test_stack_protobuf_queue_rate(max_power, message_sz);
    std::string path;
    {
        const auto kCommandLineArguments = parseCommandLineArguments(argc, argv);
        const auto &[kOwnNetworkSize, kOtherNetworkSize, kOwnMaxNumFailedStake, kOtherMaxNumFailedStake, kNodeId,
                     kLogPath, kWorkingDir] = kCommandLineArguments;
        const auto kNetworkZeroConfigPath = kWorkingDir + "network0urls.txt"s;
        const auto kNetworkOneConfigPath = kWorkingDir + "network1urls.txt"s;

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
        constexpr auto kMessageBufferSize = 1024; // Old value: 256

        const auto acknowledgment = std::make_shared<Acknowledgment>();
        const auto pipeline = std::make_shared<Pipeline>(kOwnNetworkConfiguration.kNetworkUrls,
                                                         kOtherNetworkConfiguration.kNetworkUrls, kNodeConfiguration);
        const auto messageBuffer = std::make_shared<iothread::MessageQueue>(kMessageBufferSize);
        constexpr auto kNumAckTrackers = kListSize / 2;
        const auto ackTrackers = std::make_shared<std::vector<std::unique_ptr<AcknowledgmentTracker>>>();
        for (uint64_t i = 0; i < kNumAckTrackers; i++)
        {
            ackTrackers->push_back(std::make_unique<AcknowledgmentTracker>(kNodeConfiguration.kOtherNetworkSize,
                                                                           kNodeConfiguration.kOtherMaxNumFailedStake));
        }
        const auto quorumAck = std::make_shared<QuorumAcknowledgment>(kQuorumSize);

        pipeline->startPipeline();

        const auto kTestStartTime = std::chrono::steady_clock::now();
        const auto testStartRecordTime = kTestStartTime + get_test_warmup_duration();
        const auto testEndTime = testStartRecordTime + get_test_duration();
        set_test_start(kTestStartTime);

        SPDLOG_INFO("Done setting up sockets between nodes.");

        set_priv_key();

        // if (get_rsm_id() == 1 && kNodeId == 0)
        // {
        //     SPDLOG_CRITICAL("Node {} in RSM {} Is Crashed", kNodeId, get_rsm_id());
        //     auto receiveThread = std::thread(runCrashedNodeReceiveThread, pipeline);

        //     std::this_thread::sleep_until(testEndTime);
        //     end_test();

        //     receiveThread.join();
        //     SPDLOG_CRITICAL("Crashed Node {} in RSM {} Finished Test", kNodeId, get_rsm_id());
        //     remove(kLogPath.c_str());
        //     return 0;
        // }

        auto messageRelayThread = std::thread(runGenerateMessageThread, messageBuffer, kNodeConfiguration);
        // auto relayRequestThread = std::thread(runRelayIPCRequestThread, messageBuffer, kNodeConfiguration);
        // auto relayTransactionThread =
        //     std::thread(runRelayIPCTransactionThread, "/tmp/scrooge-output", quorumAck, kNodeConfiguration);
        // SPDLOG_INFO("Created Generate message relay thread");

        auto sendThread = std::thread(runUnfairOneToOneSendThread /*runOneToOneSendThread*/, messageBuffer, pipeline,
                                      acknowledgment, ackTrackers, quorumAck, kNodeConfiguration);
        auto receiveThread =
            std::thread(runAllToAllReceiveThread, pipeline, acknowledgment, ackTrackers, quorumAck, kNodeConfiguration);
        SPDLOG_INFO("Created Receiver Thread with ID={} ");

        std::this_thread::sleep_until(testStartRecordTime);
        const auto trueTestStartTime = std::chrono::steady_clock::now();
        start_recording();
        addMetric("starting_quack", quorumAck->getCurrentQuack().value_or(0));
        addMetric("starting_ack", acknowledgment->getAckIterator().value_or(0));

        std::this_thread::sleep_until(testEndTime);
        const auto trueTestEndTime = std::chrono::steady_clock::now();
        end_test();

        messageRelayThread.join();
        sendThread.join();
        receiveThread.join();
        // relayRequestThread.join();
        // relayTransactionThread.join();

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
    }
    printMetrics(path);
    return 0;
}
