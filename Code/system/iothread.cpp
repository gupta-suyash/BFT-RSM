#include "iothread.h"

#include "crypto.h"
#include "ipc.h"
#include "scrooge_message.pb.h"
#include "scrooge_request.pb.h"
#include "scrooge_transfer.pb.h"
#include "statisticstracker.h"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <fstream>
#include <limits>
#include <map>
#include <pthread.h>
#include <stdio.h>
#include <thread>
#include <unistd.h>

#include <nng/nng.h>

bool checkMessageMac(const scrooge::CrossChainMessage& message)
{
    // Verification TODO
    const auto mstr = message.ack_count().SerializeAsString();
    // Fetch the sender key
    // const auto senderKey = get_other_rsm_key(nng_message.senderId);
    const auto senderKey = get_priv_key();
    // Verify the message
    return CmacVerifyString(senderKey, mstr, message.validity_proof());
}

bool isMessageDataValid(const scrooge::CrossChainMessageData &message)
{
    // no signature checking currently
    return true;
}

// Generates fake messages of a given size for throughput testing
void runGenerateMessageThread(const std::shared_ptr<iothread::MessageQueue> messageOutput,
                              const NodeConfiguration configuration)
{
    bindThreadToCpu(3);
    const auto kMessageSize = get_packet_size();

    for (uint64_t curSequenceNumber = 0; not is_test_over(); curSequenceNumber++)
    {
        scrooge::CrossChainMessageData fakeData;
        fakeData.set_message_content(std::string(kMessageSize, 'L'));
        fakeData.set_sequence_number(curSequenceNumber);

        while (not messageOutput->try_enqueue(std::move(fakeData)) && not is_test_over())
        {
            std::this_thread::sleep_for(10us);
        }
    }
}

// Generates fake messages of a given size for throughput testing
void runGenerateMessageThreadWithIpc()
{
    auto fork_res = fork();
    if (fork_res > 0)
    {
        return;
    }
    else if (fork_res < 0)
    {
        SPDLOG_CRITICAL("CANNOT FORK PROCESS ERR {}", fork_res);
    }

    constexpr auto kScroogeInputPath = "/tmp/scrooge-input";
    constexpr auto kMessagesPerSecond = 8000;
    constexpr auto kBurstSize = 1000;
    constexpr auto kMessageSpace = 1.0s * kBurstSize / kMessagesPerSecond;

    createPipe(kScroogeInputPath);

    std::ofstream pipe{kScroogeInputPath};
    if (!pipe.is_open())
    {
        SPDLOG_CRITICAL("Writer Open Failed={}, {}", std::strerror(errno), getlogin());
    }
    else
    {
        SPDLOG_CRITICAL("Writer Open Success");
    }

    const auto kMessageSize = get_packet_size();
    auto lastSendTime = std::chrono::steady_clock::now();

    uint64_t curSequenceNumber = 0;
    // for (uint64_t curSequenceNumber = 0; not is_test_over(); curSequenceNumber++)
    while (not is_test_over())
    {
        while (std::chrono::steady_clock::now() - lastSendTime < kMessageSpace)
            ;
        lastSendTime = std::chrono::steady_clock::now();

        scrooge::ScroogeRequest request;
        for (uint64_t burst = 0; burst < kBurstSize; burst++)
        {
            auto messageContent = request.mutable_send_message_request()->mutable_content();
            messageContent->set_message_content(std::string(kMessageSize, 'L'));
            messageContent->set_sequence_number(curSequenceNumber);
            curSequenceNumber++;

            writeMessage(pipe, request.SerializeAsString());
        }
    }
}

// Relays messages to be sent over ipc
void runRelayIPCRequestThread(const std::shared_ptr<iothread::MessageQueue> messageOutput,
                              NodeConfiguration kNodeConfiguration)
{
    bindThreadToCpu(3);
    constexpr auto kScroogeInputPath = "/tmp/scrooge-input";
    Acknowledgment receivedMessages{};
    uint64_t numReceivedMessages{};

    createPipe(kScroogeInputPath);
    std::ifstream pipe{kScroogeInputPath};
    if (!pipe.is_open())
    {
        SPDLOG_CRITICAL("Reader Open Failed={}, {}", std::strerror(errno), getlogin());
    }
    else
    {
        SPDLOG_CRITICAL("Reader Open Success");
    }

    while (not is_test_over())
    {
        auto messageBytes = readMessage(pipe);

        scrooge::ScroogeRequest newRequest;

        const auto isParseSuccessful = newRequest.ParseFromString(std::move(messageBytes));
        if (not isParseSuccessful)
        {
            SPDLOG_CRITICAL("FAILED TO READ MESSAGE");
            continue;
        }

        switch (newRequest.request_case())
        {
            using request = scrooge::ScroogeRequest::RequestCase;
        case request::kSendMessageRequest: {
            auto newMessageRequest = newRequest.send_message_request();
            receivedMessages.addToAckList(newMessageRequest.content().sequence_number());

            while (not messageOutput->try_enqueue(std::move(*(newMessageRequest.mutable_content()))) &&
                   not is_test_over())
                std::this_thread::sleep_for(10us);
            break;
        }
        default: {
            SPDLOG_ERROR("UNKNOWN REQUEST TYPE {}", newRequest.request_case());
        }
        }
    }

    addMetric("ipc_recv_messages", numReceivedMessages);
    addMetric("ipc_msg_block_size", receivedMessages.getAckIterator().value_or(0));
    SPDLOG_INFO("Relay IPC Message Thread Exiting");
}

void runAllToAllSendThread(const std::shared_ptr<iothread::MessageQueue> messageInput,
                           const std::shared_ptr<Pipeline> pipeline,
                           const std::shared_ptr<Acknowledgment> acknowledgment,
                           const std::shared_ptr<std::vector<std::unique_ptr<AcknowledgmentTracker>>> ackTrackers,
                           const std::shared_ptr<QuorumAcknowledgment> quorumAck, const NodeConfiguration configuration)
{
    // bindThreadToCpu(0);
    SPDLOG_CRITICAL("Send Thread starting with TID = {}", gettid());

    uint64_t numMessagesSent{};
    Acknowledgment sentMessages{};
    while (not is_test_over())
    {
        scrooge::CrossChainMessageData newMessageData;
        while (messageInput->try_dequeue(newMessageData) && not is_test_over())
        {
            const auto curSequenceNumber = newMessageData.sequence_number();
            auto curTime = std::chrono::steady_clock::now();

            pipeline->SendToAllOtherRsm(std::move(newMessageData), curTime);
            sentMessages.addToAckList(curSequenceNumber);
            quorumAck->updateNodeAck(0, 0ULL - 1, sentMessages.getAckIterator().value_or(0));
            numMessagesSent++;

            allToall(curTime);
        }
    }

    addMetric("Latency", averageLat());
    addMetric("transfer_strategy", "All-to-All");
    addMetric("num_msgs_sent", numMessagesSent);
    // addMetric("max_quack", quorumAck->getCurrentQuack().value_or(0));
    SPDLOG_INFO("ALL CROSS CONSENSUS PACKETS SENT : send thread exiting");
}

void runOneToOneSendThread(const std::shared_ptr<iothread::MessageQueue> messageInput,
                           const std::shared_ptr<Pipeline> pipeline,
                           const std::shared_ptr<Acknowledgment> acknowledgment,
                           const std::shared_ptr<std::vector<std::unique_ptr<AcknowledgmentTracker>>> ackTrackers,
                           const std::shared_ptr<QuorumAcknowledgment> quorumAck, const NodeConfiguration configuration)
{
    // bindThreadToCpu(0);
    SPDLOG_CRITICAL("Send Thread starting with TID = {}", gettid());
    const auto &[kOwnNetworkSize, kOtherNetworkSize, kOwnNetworkStakes, kOtherNetworkStakes, kOwnMaxNumFailedStake,
                 kOtherMaxNumFailedStake, kNodeId, kLogPath, kWorkingDir] = configuration;

    uint64_t numMessagesSent{};
    Acknowledgment sentMessages{};
    while (not is_test_over())
    {
        scrooge::CrossChainMessageData newMessageData;
        while (messageInput->try_dequeue(newMessageData) && not is_test_over())
        {
            const auto curSequenceNumber = newMessageData.sequence_number();
            auto curTime = std::chrono::steady_clock::now();

            pipeline->SendToOtherRsm(kNodeId % kOtherNetworkSize, std::move(newMessageData), nullptr, curTime);
            sentMessages.addToAckList(curSequenceNumber);
            quorumAck->updateNodeAck(0, 0ULL - 1, sentMessages.getAckIterator().value_or(0));
            numMessagesSent++;

            allToall(curTime);
        }
    }

    addMetric("Latency", averageLat());
    addMetric("transfer_strategy", "One-to-One");
    addMetric("num_msgs_sent", numMessagesSent);
    SPDLOG_INFO("ALL CROSS CONSENSUS PACKETS SENT : send thread exiting");
}

void runSendThread(const std::shared_ptr<iothread::MessageQueue> messageInput, const std::shared_ptr<Pipeline> pipeline,
                   const std::shared_ptr<Acknowledgment> acknowledgment,
                   const std::shared_ptr<std::vector<std::unique_ptr<AcknowledgmentTracker>>> ackTrackers,
                   const std::shared_ptr<QuorumAcknowledgment> quorumAck, const NodeConfiguration configuration)
{
    SPDLOG_CRITICAL("SEND THREAD TID {}", gettid());
    const auto &[kOwnNetworkSize, kOtherNetworkSize, kOwnNetworkStakes, kOtherNetworkStakes, kOwnMaxNumFailedStake,
                 kOtherMaxNumFailedStake, kNodeId, kLogPath, kWorkingDir] = configuration;

    bindThreadToCpu(2);
    SPDLOG_INFO("Send Thread starting with TID = {}", gettid());

    const MessageScheduler messageScheduler(configuration);

    uint64_t numMessagesResent{};

    boost::circular_buffer<iothread::MessageResendData> resendDatas(1 << 16);
    std::vector<acknowledgment_tracker::ResendData> activeResends(ackTrackers->size());

    auto lastSendTime = std::chrono::steady_clock::now();
    auto lastNoopTime = std::chrono::steady_clock::now();
    uint64_t numMsgsSentWithLastAck{};
    std::optional<uint64_t> lastSentAck{};
    uint64_t lastQuack = 0;
    constexpr uint64_t kAckWindowSize = 3 * 2 * 4 * 2;
    constexpr uint64_t kQAckWindowSize = kListSize * 100;
    // Optimal window size for non-stake: 12*16 and for stake: 12*8
    constexpr auto kMaxMessageDelay = 2us;
    constexpr auto kNoopDelay = 500us;
    constexpr auto kAckTrackerTimeout = .5ms;
    std::chrono::steady_clock::time_point lastAckTrackerCheck{};
    // kNoopDelay for Scrooge for non-failures: 500
    uint64_t noop_ack = 0;
    uint64_t numResendChecks{}, numActiveResends{}, numResendsOverQuack{}, numMessagesSent{};
    uint64_t numQuackWindowFails{}, numAckWindowFails{}, numSendChecks{}, numTimeoutHits{}, numNoopTimeoutHits{},
        numTimeoutExclusiveHits{};
    while (not is_test_over())
    {
        // update window information
        const auto curAck = acknowledgment->getAckIterator();
        const auto curQuack = quorumAck->getCurrentQuack();

        if (curAck != lastSentAck)
        {
            lastSentAck = curAck;
            numMsgsSentWithLastAck = 0;
        }

        const int64_t pendingSequenceNum =
            (messageInput->peek()) ? messageInput->peek()->sequence_number() : kQAckWindowSize + curQuack.value_or(0);
        const bool isAckFresh = numMsgsSentWithLastAck < kAckWindowSize;
        const bool isSequenceNumberUseful = pendingSequenceNum - curQuack.value_or(0ULL - 1) < kQAckWindowSize;
        const auto curTime = std::chrono::steady_clock::now();
        const bool isNoopTimeoutHit = curTime - std::max(lastNoopTime, lastSendTime) > kNoopDelay;
        const bool isTimeoutHit = curTime - lastSendTime > kMaxMessageDelay;
        const bool shouldDequeue = isTimeoutHit || (isAckFresh && isSequenceNumberUseful);

        numSendChecks++;
        numQuackWindowFails += not isSequenceNumberUseful;
        numAckWindowFails += not isAckFresh;
        numTimeoutHits += isTimeoutHit;
        numTimeoutExclusiveHits += isTimeoutHit && not(isSequenceNumberUseful && isAckFresh);
        numNoopTimeoutHits += isNoopTimeoutHit;

        scrooge::CrossChainMessageData newMessageData;
        if (not resendDatas.full() && shouldDequeue && messageInput->try_dequeue(newMessageData) && not is_test_over())
        {
            const auto sequenceNumber = newMessageData.sequence_number();

            // Start the timer for latency
            startTimer(sequenceNumber, curTime);

            // Recording latency
            uint64_t checkVal = curQuack.value_or(0);
            recordLatency(checkVal, curTime);
            lastQuack = checkVal;

            const auto resendNumber = messageScheduler.getResendNumber(sequenceNumber);
            const auto isMessageNeverSent = not resendNumber.has_value();
            if (isMessageNeverSent)
            {
                continue;
            }

            auto destinations = messageScheduler.getMessageDestinations(sequenceNumber);

            bool isFirstSender = resendNumber == 0;
            const auto isPossiblySentLater = not isFirstSender || destinations.size() > 1;
            if (isFirstSender)
            {
                const auto receiverNode = destinations.at(0);
                numMessagesSent++;

                if (isPossiblySentLater)
                {
                    auto messageDataCopy = newMessageData;
                    numMsgsSentWithLastAck += pipeline->SendToOtherRsm(receiverNode, std::move(messageDataCopy),
                                                                       acknowledgment.get(), curTime);
                }
                else
                {
                    numMsgsSentWithLastAck += pipeline->SendToOtherRsm(receiverNode, std::move(newMessageData),
                                                                       acknowledgment.get(), curTime);
                }
                lastSendTime = curTime;
            }

            if (isPossiblySentLater)
            {
                const uint64_t numDestinationsAlreadySent = isFirstSender;
                resendDatas.push_back(iothread::MessageResendData{.sequenceNumber = sequenceNumber,
                                                                  .firstDestinationResendNumber = resendNumber.value(),
                                                                  .numDestinationsSent = numDestinationsAlreadySent,
                                                                  .messageData = std::move(newMessageData),
                                                                  .destinations = destinations});
            }
        }
        else if (isAckFresh && isNoopTimeoutHit)
        {
            static uint64_t receiver = 0;

            pipeline->SendToOtherRsm(receiver % kOtherNetworkSize, {}, acknowledgment.get(), curTime);

            receiver++;
            numMsgsSentWithLastAck++;
            noop_ack++;

            lastSendTime = curTime;
        }

        constexpr auto kResendBlockSize = 4096;
        if (resendDatas.size() > kResendBlockSize && resendDatas.at(kResendBlockSize).sequenceNumber <= curQuack)
        {
            resendDatas.erase_begin(kResendBlockSize);
        }

        // Check resends
        if (curTime - lastAckTrackerCheck < kAckTrackerTimeout && not resendDatas.full())
        {
            continue;
        }

        lastAckTrackerCheck = curTime;
        activeResends.clear();

        if (resendDatas.empty())
        {
            continue;
        }

        auto pseudoBegin = std::begin(resendDatas);
        uint64_t prevActiveResendSequenceNum = 0;

        std::bitset<32> destinationsToFlush{};

        for (const auto &ackTracker : *ackTrackers)
        {
            const auto activeResend = ackTracker->getActiveResendData();
            numResendChecks++;
            if (not activeResend.isActive)
            {
                continue;
            }

            numActiveResends++;

            const bool isTooLow = activeResend.sequenceNumber < resendDatas.front().sequenceNumber;
            if (isTooLow)
            {
                continue;
            }
            numResendsOverQuack++;

            const auto curActiveResendSequenceNum = activeResend.sequenceNumber;
            const bool isMonotone = prevActiveResendSequenceNum <= curActiveResendSequenceNum;
            prevActiveResendSequenceNum = curActiveResendSequenceNum;
            auto curResendDataIt = pseudoBegin;
            if (isMonotone)
            {
                curResendDataIt = std::find_if(pseudoBegin, std::end(resendDatas),
                                               [&](const iothread::MessageResendData &possibleData) -> bool {
                                                   return possibleData.sequenceNumber >= activeResend.sequenceNumber;
                                               });
            }
            else
            {
                curResendDataIt = std::find_if(std::begin(resendDatas), pseudoBegin,
                                               [&](const iothread::MessageResendData &possibleData) -> bool {
                                                   return possibleData.sequenceNumber >= activeResend.sequenceNumber;
                                               });
            }
            pseudoBegin = curResendDataIt;

            if (curResendDataIt == std::end(resendDatas))
            {
                continue;
            }

            const bool noResendDataFound = curResendDataIt->sequenceNumber > activeResend.sequenceNumber;
            const bool alreadySent = curResendDataIt->messageData.message_content().empty();
            if (noResendDataFound || alreadySent)
            {
                continue;
            }

            auto &[sequenceNumber, firstDestinationResendNumber, numDestinationsSent, messageData, destinations] =
                *curResendDataIt;

            // We got to the message which should be resent
            const auto curNodeFirstResend = firstDestinationResendNumber;
            const auto curNodeLastResend = firstDestinationResendNumber + destinations.size() - 1;
            const auto curNodeCompletedResends = curNodeFirstResend + numDestinationsSent;
            const auto curFinalDestination = std::min<uint64_t>(curNodeLastResend, activeResend.resendNumber);
            for (uint64_t resend = curNodeCompletedResends; resend <= curFinalDestination; resend++)
            {
                const auto destination = destinations.at(resend - curNodeFirstResend);
                const bool isSentLater = numDestinationsSent + 1 < destinations.size();
                if (isSentLater)
                {
                    scrooge::CrossChainMessageData messageDataCopy;
                    messageDataCopy.CopyFrom(messageData);
                    bool isFlushed = pipeline->SendToOtherRsm(destination, std::move(messageDataCopy),
                                                              acknowledgment.get(), curTime);
                    destinationsToFlush[destination] = not isFlushed;
                }
                else
                {
                    bool isFlushed =
                        pipeline->SendToOtherRsm(destination, std::move(messageData), acknowledgment.get(), curTime);
                    destinationsToFlush[destination] = not isFlushed;
                }
                numDestinationsSent++;
                numMessagesResent += is_test_recording();
            }
        }

        uint64_t numDeletes = 0;
        for (; numDeletes < resendDatas.size(); numDeletes++)
        {
            const auto &curResend = resendDatas[numDeletes];
            if (curResend.numDestinationsSent != curResend.destinations.size())
            {
                break;
            }
        }
        if (numDeletes > 64)
            resendDatas.erase_begin(numDeletes);
    }

    addMetric("Noop Acks", noop_ack);
    addMetric("Noop Delay", std::chrono::duration<double>(kNoopDelay).count());
    addMetric("Ack Window", kAckWindowSize);
    addMetric("Quack Window", kQAckWindowSize);
    addMetric("Latency", averageLat());
    addMetric("transfer_strategy", "Scrooge k=" + std::to_string(kListSize) + "+ resends");
    addMetric("num_msgs_sent_primary", numMessagesSent);
    addMetric("num_msgs_resent", numMessagesResent);
    addMetric("num_resend_checks", numResendChecks);
    addMetric("avg_resend_active", (double)numActiveResends / (double)numResendChecks);
    addMetric("avg_resend_active_over_quack", (double)numResendsOverQuack / (double)numResendChecks);
    addMetric("Quack-Fail", (double)numQuackWindowFails / numSendChecks);
    addMetric("Ack-Fail", (double)numAckWindowFails / numSendChecks);
    addMetric("Timeout-Hits", (double)numTimeoutHits / numSendChecks);
    addMetric("Noop-Hits", (double)numNoopTimeoutHits / numSendChecks);
    addMetric("Timeout-Exclusive-Hits", (double)numTimeoutExclusiveHits / numSendChecks);

    SPDLOG_INFO("ALL CROSS CONSENSUS PACKETS SENT : send thread exiting");
}

void runRelayIPCTransactionThread(std::string scroogeOutputPipePath, std::shared_ptr<QuorumAcknowledgment> quorumAck,
                                  NodeConfiguration kNodeConfiguration)
{
    std::ofstream pipe{scroogeOutputPipePath, std::ios_base::app};
    if (!pipe.is_open())
    {
        SPDLOG_CRITICAL("Open Failed={}, {}", std::strerror(errno), getlogin());
    }
    else
    {
        SPDLOG_CRITICAL("Open Success");
    }

    std::optional<uint64_t> lastQuorumAck{};
    scrooge::ScroogeTransfer transfer;
    const auto mutableCommitAck = transfer.mutable_commit_acknowledgment();
    while (not is_test_over())
    {
        const auto curQuorumAck = quorumAck->getCurrentQuack();
        if (lastQuorumAck < curQuorumAck)
        {
            lastQuorumAck = curQuorumAck;
            mutableCommitAck->set_sequence_number(lastQuorumAck.value());
            const auto serializedTransfer = transfer.SerializeAsString();
            // SPDLOG_CRITICAL("Write: {} :: N:{} :: R:{}",lastQuorumAck.value(), kNodeConfiguration.kNodeId,
            // get_rsm_id());
            writeMessage(pipe, serializedTransfer);
        }
    }
    pipe.close();
    addMetric("IPC test", true);
}

uint64_t numMissing = 0;
uint64_t numChecked = 0;
uint64_t numRecv = 0;
void updateAckTrackers(const std::optional<uint64_t> curQuack, const uint64_t nodeId, const uint64_t nodeStake,
                       const acknowledgment::AckView<kListSize> nodeAckView,
                       std::vector<std::unique_ptr<AcknowledgmentTracker>> *const ackTrackers)
{
    numRecv++;
    const auto kNumAckTrackers = ackTrackers->size();
    const auto initialMessageTrack = curQuack.value_or(0ULL - 1ULL) + 1;
    const auto finialMessageTrack =
        std::min(initialMessageTrack + kNumAckTrackers - 1, acknowledgment::getFinalAck(nodeAckView));
    for (uint64_t curMessage = initialMessageTrack; curMessage <= finialMessageTrack; curMessage++)
    {
        numChecked++;
        const auto virtualQuack = (curMessage) ? std::optional<uint64_t>(curMessage - 1) : std::nullopt;
        const auto isNodeMissingCurMessage = not acknowledgment::testAckView(nodeAckView, curMessage);
        const auto curAckTracker = ackTrackers->data() + (curMessage % ackTrackers->size());

        if (isNodeMissingCurMessage)
        {
            numMissing++;
            (*curAckTracker)->update(nodeId, nodeStake, virtualQuack, virtualQuack);
        }
        else
        {
            (*curAckTracker)->update(nodeId, nodeStake, virtualQuack.value_or(0ULL - 1ULL) + 1, virtualQuack);
        }
    }
}

struct LameAckData
{
    uint8_t senderId;
    acknowledgment::AckView<kListSize> senderAckView;
};
void lameAckThread(Acknowledgment *const acknowledgment, QuorumAcknowledgment *const quorumAck,
                   std::vector<std::unique_ptr<AcknowledgmentTracker>> *const ackTrackers,
                   moodycamel::BlockingReaderWriterCircularBuffer<LameAckData> *const viewQueue,
                   const NodeConfiguration configuration)
{
    bindThreadToCpu(1);
    SPDLOG_CRITICAL("STARTING LAME THREAD {}", gettid());
    while (not is_test_over())
    {
        LameAckData curData{};
        while (not viewQueue->try_dequeue(curData) && not is_test_over())
            ;

        const auto &[senderId, senderAckView] = curData;

        const auto curForeignAck = acknowledgment::getAckIterator(senderAckView);

        const auto senderStake = configuration.kOtherNetworkStakes.at(senderId);
        if (curForeignAck.has_value())
        {
            quorumAck->updateNodeAck(senderId, senderStake, *curForeignAck);
        }

        const auto currentQuack = quorumAck->getCurrentQuack();
        updateAckTrackers(currentQuack, senderId, senderStake, senderAckView, ackTrackers);
    }
    SPDLOG_CRITICAL("LAME THREAD ENDING");
}

void runReceiveThread(const std::shared_ptr<Pipeline> pipeline, const std::shared_ptr<Acknowledgment> acknowledgment,
                      const std::shared_ptr<std::vector<std::unique_ptr<AcknowledgmentTracker>>> ackTrackers,
                      const std::shared_ptr<QuorumAcknowledgment> quorumAck, const NodeConfiguration configuration)
{
    bindThreadToCpu(0);
    SPDLOG_CRITICAL("RECV THREAD TID {}", gettid());

    uint64_t timedMessages{};
    pipeline::ReceivedCrossChainMessage receivedMessage{};

    scrooge::CrossChainMessage crossChainMessage;
    auto ackView = std::array<uint64_t, acknowledgment::AckView<kListSize>::kNumInts>{};

    moodycamel::BlockingReaderWriterCircularBuffer<LameAckData> viewQueue(1 << 12);
    std::thread lameThread(lameAckThread, acknowledgment.get(), quorumAck.get(), ackTrackers.get(), &viewQueue,
                           configuration);

    while (not is_test_over())
    {
        if (receivedMessage.message == nullptr)
        {
            receivedMessage = pipeline->RecvFromOtherRsm();

            if (receivedMessage.message != nullptr)
            {
                const auto [message, senderId] = receivedMessage;

                const auto messageData = nng_msg_body(message);
                const auto messageSize = nng_msg_len(message);
                bool success = crossChainMessage.ParseFromArray(messageData, messageSize);
                if (not success)
                {
                    SPDLOG_CRITICAL("Cannot parse foreign message");
                }
                
                bool isMessageValid = checkMessageMac(crossChainMessage);
                if (not isMessageValid)
                {
                    SPDLOG_CRITICAL("Received invalid mac");
                    nng_msg_free(receivedMessage.message);
                    receivedMessage.message = nullptr;
                    receivedMessage.senderId = 0;
                    continue;
                }

                for (const auto &messageData : crossChainMessage.data())
                {
                    if (not isMessageDataValid(messageData))
                    {
                        continue;
                    }
                    acknowledgment->addToAckList(messageData.sequence_number());
                    timedMessages += is_test_recording();
                }

                const auto curForeignAck = (crossChainMessage.has_ack_count())
                                               ? std::optional<uint64_t>(crossChainMessage.ack_count().value())
                                               : std::nullopt;
                assert(ackView.size() == crossChainMessage.ack_set_size() && "AckListSize inconsistent");
                std::copy_n(crossChainMessage.ack_set().begin(), ackView.size(), ackView.begin());
                const auto senderAckView = acknowledgment::AckView<kListSize>{
                    .ackOffset = curForeignAck.value_or(0ULL - 1ULL) + 2, .view = ackView};

                while (not viewQueue.try_enqueue({.senderId = (uint8_t)senderId, .senderAckView = senderAckView}) &&
                       not is_test_over())
                    ;
            }
        }

        if (receivedMessage.message)
        {
            bool success = pipeline->rebroadcastToOwnRsm(receivedMessage.message);
            if (success)
            {
                receivedMessage.message = nullptr;
            }
        }

        const auto [message, senderId] = pipeline->RecvFromOwnRsm();

        if (message)
        {
            const auto messageData = nng_msg_body(message);
            const auto messageSize = nng_msg_len(message);
            bool success = crossChainMessage.ParseFromArray(messageData, messageSize);
            nng_msg_free(message);
            if (not success)
            {
                SPDLOG_CRITICAL("Cannot parse local message");
            }

            for (const auto &messageData : crossChainMessage.data())
            {
                if (not isMessageDataValid(messageData))
                {
                    continue;
                }
                acknowledgment->addToAckList(messageData.sequence_number());
                timedMessages += is_test_recording();
            }
        }
    }

    if (receivedMessage.message)
    {
        nng_msg_free(receivedMessage.message);
    }

    lameThread.join();

    SPDLOG_INFO("ALL MESSAGES RECEIVED : Receive thread exiting");
    addMetric("Average Missing Acks", (double)numMissing / (double)numChecked);
    addMetric("Average KList Size", (double)numChecked / (double)numRecv);
    addMetric("NumKListRecv", numRecv);
    addMetric("foreign_messages_received", timedMessages);
    addMetric("max_acknowledgment", acknowledgment->getAckIterator().value_or(0));
    addMetric("max_quorum_acknowledgment", quorumAck->getCurrentQuack().value_or(0));
}

void runAllToAllReceiveThread(const std::shared_ptr<Pipeline> pipeline,
                              const std::shared_ptr<Acknowledgment> acknowledgment,
                              const std::shared_ptr<std::vector<std::unique_ptr<AcknowledgmentTracker>>> ackTrackers,
                              const std::shared_ptr<QuorumAcknowledgment> quorumAck,
                              const NodeConfiguration configuration)
{
    SPDLOG_CRITICAL("RECV THREAD TID {}", gettid());
    uint64_t timedMessages{};
    scrooge::CrossChainMessage crossChainMessage;

    while (not is_test_over())
    {
        const auto [message, senderId] = pipeline->RecvFromOtherRsm();
        if (not message)
        {
            std::this_thread::yield();
            continue;
        }

        const auto messageData = nng_msg_body(message);
        const auto messageSize = nng_msg_len(message);
        bool success = crossChainMessage.ParseFromArray(messageData, messageSize);
        nng_msg_free(message);
        if (not success)
        {
            SPDLOG_CRITICAL("Cannot parse foreign message");
        }

        for (const auto &messageData : crossChainMessage.data())
        {
            if (not isMessageDataValid(messageData))
            {
                continue;
            }
            acknowledgment->addToAckList(messageData.sequence_number());
            timedMessages += is_test_recording();
        }
    }

    addMetric("local_messages_received", 0);
    addMetric("foreign_messages_received", timedMessages);
    addMetric("max_acknowledgment", acknowledgment->getAckIterator().value_or(0));
    addMetric("max_quorum_acknowledgment", quorumAck->getCurrentQuack().value_or(0));
}

void runCrashedNodeReceiveThread(const std::shared_ptr<Pipeline> pipeline)
{
    while (not is_test_over())
    {
        // steal other node's bandwidth but don't help in protocol
        auto [foreignMessage, foreignSender] = pipeline->RecvFromOtherRsm();
        if (foreignMessage)
        {
            nng_msg_free(foreignMessage);
        }
        auto [localMessage, localSender] = pipeline->RecvFromOwnRsm();
        if (localMessage)
        {
            nng_msg_free(localMessage);
        }
    }
}
