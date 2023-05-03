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

bool checkMessageMac(const scrooge::CrossChainMessage *const message)
{
    // Verification TODO
    const auto mstr = message->ack_count().SerializeAsString();
    // Fetch the sender key
    // const auto senderKey = get_other_rsm_key(nng_message.senderId);
    const auto senderKey = get_priv_key();
    // Verify the message
    return CmacVerifyString(senderKey, mstr, message->validity_proof());
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
    const auto kMessageSize = get_packet_size();

    for (uint64_t curSequenceNumber = 0; not is_test_over(); curSequenceNumber++)
    {
        scrooge::CrossChainMessageData fakeData;
        fakeData.set_message_content(std::string(kMessageSize, 'L'));
        fakeData.set_sequence_number(curSequenceNumber);

        while (not messageOutput->wait_enqueue_timed(std::move(fakeData), 100ms) && not is_test_over())
            ;
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

            while (not messageOutput->wait_enqueue_timed(std::move(*(newMessageRequest.mutable_content())), 1ms) && not is_test_over())
                ;
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
            auto start_time = std::chrono::steady_clock::now();

            pipeline->SendToAllOtherRsm(std::move(newMessageData));
            sentMessages.addToAckList(curSequenceNumber);
            quorumAck->updateNodeAck(0, 0ULL - 1, sentMessages.getAckIterator().value_or(0));
            numMessagesSent++;

            allToall(start_time);
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
            auto start_time = std::chrono::steady_clock::now();

            pipeline->SendToOtherRsm(kNodeId % kOtherNetworkSize, std::move(newMessageData), nullptr);
            sentMessages.addToAckList(curSequenceNumber);
            quorumAck->updateNodeAck(0, 0ULL - 1, sentMessages.getAckIterator().value_or(0));
            numMessagesSent++;

            allToall(start_time);
        }
    }

    addMetric("Latency", averageLat());
    addMetric("transfer_strategy", "One-to-One");
    addMetric("num_msgs_sent", numMessagesSent);
    SPDLOG_INFO("ALL CROSS CONSENSUS PACKETS SENT : send thread exiting");
}

void lameResendDataThread(
    moodycamel::BlockingReaderWriterCircularBuffer<iothread::MessageResendData>* const resendDataInput,
    QuorumAcknowledgment* const quorumAck,
    const std::vector<std::unique_ptr<AcknowledgmentTracker>>* const ackTrackers,
    Pipeline* const pipeline
)
{
    bindThreadToCpu(0);
    uint64_t numResendChecks{}, numActiveResends{}, numResendsOverQuack{}, numMessagesResent{}, preSentResends{};

    boost::circular_buffer<iothread::MessageResendData> resendDatas(1<<20);
    std::vector<acknowledgment_tracker::ResendData> activeResends(ackTrackers->size());

    iothread::MessageResendData newData;

    std::chrono::microseconds timeSearching{};
    uint64_t numSearches{};


    SPDLOG_CRITICAL("LameResendDataThread starting {}", gettid());
    while (not is_test_over())
    {
        activeResends.clear(); // clear old resend data
        while (not resendDatas.full() && resendDataInput->try_dequeue(newData))
        {
            assert((resendDatas.empty() || resendDatas.back().sequenceNumber < newData.sequenceNumber) && "Resend data nonmonotone");
            resendDatas.push_back(std::move(newData));
        }

        if (resendDatas.empty())
        {
            continue;
        }

        const auto curQuack = quorumAck->getCurrentQuack();
        const auto endOfOldData = std::find_if(std::cbegin(resendDatas), std::cend(resendDatas), [&](auto& resendData) -> bool {
            return resendData.sequenceNumber > curQuack;
        });
        const auto numToErase = std::distance(std::cbegin(resendDatas), endOfOldData);
        resendDatas.erase_begin(numToErase);

        if (resendDatas.empty())
        {
            continue;
        }


        auto pseudoBegin = std::begin(resendDatas);
        uint64_t prevActiveResendSequenceNum = 0;

        const auto searchStart =  std::chrono::steady_clock::now();
        numSearches++;
        for (const auto& ackTracker : *ackTrackers)
        {
            const auto activeResend = ackTracker->getActiveResendData();
            numResendChecks++;
            if (not activeResend.isActive)
            {
                continue;
            }

            numActiveResends++;

            const bool isTooLow = activeResend.sequenceNumber < resendDatas.front().sequenceNumber;
            const bool isTooHigh = activeResend.sequenceNumber > resendDatas.back().sequenceNumber;
            if (isTooLow || isTooHigh)
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
                curResendDataIt  = std::find_if(pseudoBegin, std::end(resendDatas), [&](const iothread::MessageResendData& possibleData) -> bool {
                    return possibleData.sequenceNumber >= activeResend.sequenceNumber;
                });
            }
            else
            {
                curResendDataIt  = std::find_if(std::begin(resendDatas), pseudoBegin, [&](const iothread::MessageResendData& possibleData) -> bool {
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

                preSentResends += alreadySent & is_test_recording();
                continue;
            }

            auto& [sequenceNumber, firstDestinationResendNumber, numDestinationsSent, messageData, destinations] = *curResendDataIt;

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
                    pipeline->AppendToSend(destination, std::move(messageDataCopy));
                }
                else
                {
                    pipeline->AppendToSend(destination, std::move(messageData));
                    assert(messageData.message_content().empty());
                }
                numDestinationsSent++;
                numMessagesResent += is_test_recording();
            }
        }
        const auto searchEnd = std::chrono::steady_clock::now();
        timeSearching += std::chrono::duration_cast<std::chrono::microseconds>(searchEnd - searchStart);


        while (not resendDatas.empty() && resendDatas.front().messageData.message_content().empty())
        {
            resendDatas.pop_front();
        }
    }
    addMetric("num_resend_checks", numResendChecks);
    addMetric("avg_resend_active", (double) numActiveResends / (double)numResendChecks);
    addMetric("avg_resend_active_over_quack", (double) numResendsOverQuack / (double)numResendChecks);
    addMetric("avg_resend_active_over_quack", (double) numResendsOverQuack / (double)numResendChecks);
    addMetric("avg_msgs_already_resent", (double) preSentResends / (double) (numMessagesResent+preSentResends));
    addMetric("avg_search_time", std::chrono::duration<double>(timeSearching / numSearches).count());
    addMetric("num_msgs_resent", numMessagesResent);
}

void runSendThread(const std::shared_ptr<iothread::MessageQueue> messageInput, const std::shared_ptr<Pipeline> pipeline,
                   const std::shared_ptr<Acknowledgment> acknowledgment,
                   const std::shared_ptr<std::vector<std::unique_ptr<AcknowledgmentTracker>>> ackTrackers,
                   const std::shared_ptr<QuorumAcknowledgment> quorumAck, const NodeConfiguration configuration)
{
    // bindThreadToCpu(2);
    SPDLOG_CRITICAL("SEND THREAD TID {}", gettid());
    const auto &[kOwnNetworkSize, kOtherNetworkSize, kOwnNetworkStakes, kOtherNetworkStakes, kOwnMaxNumFailedStake,
                 kOtherMaxNumFailedStake, kNodeId, kLogPath, kWorkingDir] = configuration;

    // bindThreadToCpu(0);
    SPDLOG_INFO("Send Thread starting with TID = {}", gettid());

    const MessageScheduler messageScheduler(configuration);

    moodycamel::BlockingReaderWriterCircularBuffer<iothread::MessageResendData>  resendMsgQueue(1<<20);
    uint64_t numMsgsFlushed{};
    auto lastNoopTime = std::chrono::steady_clock::now();
    auto lastMessageSendTime = std::chrono::steady_clock::now();
    std::optional<uint64_t> lastSentAck{};
    uint64_t numMsgsSentWithLastAck{};
    const uint64_t kAckWindowSize = configuration.kOtherNetworkSize * 2;
    const uint64_t kQAckWindowSize = 2 * kListSize + 1;
    
    constexpr auto kNoopDelay = 999'999'999'999ms;
    constexpr auto kMaxMessageSpace = 1ms;

    std::thread lameResendThread(lameResendDataThread, &resendMsgQueue, quorumAck.get(), ackTrackers.get(), pipeline.get());

    // kNoopDelay for Scrooge for non-failures: 500
    uint64_t noop_ack{}, numMessagesSent{};

    while (not is_test_over())
    {
        // update window information
        const auto curTime = std::chrono::steady_clock::now();
        const auto curAck = acknowledgment->getAckIterator();
        const auto curQuack = quorumAck->getCurrentQuack();
        recordLatency(curQuack.value_or(0), curTime);

        if (curAck != lastSentAck)
        {
            lastSentAck = curAck;
            numMsgsSentWithLastAck = 0;
        }

        const uint64_t numFlushedMessages = pipeline->flushBuffers(acknowledgment.get(), curTime);
        numMsgsSentWithLastAck += numFlushedMessages;
        if (numFlushedMessages >=  1)
        {
            numMsgsFlushed += numFlushedMessages;
            lastMessageSendTime = curTime;
            numMsgsSentWithLastAck += numFlushedMessages;
        }

        const int64_t pendingSequenceNum = (messageInput->peek()) ? messageInput->peek()->sequence_number()
                                                                  : kQAckWindowSize + curQuack.value_or(0ULL - 1) + 1;
        const bool isAckUseful = numMsgsSentWithLastAck < kAckWindowSize;
        const bool isSequenceNumberUseful = pendingSequenceNum <= kQAckWindowSize + curQuack.value_or(0ULL - 1);
        const bool isNoopTimeoutHit = curTime - lastNoopTime > kNoopDelay;
        const bool isMaxTimeoutHit = curTime - lastMessageSendTime > kMaxMessageSpace;
        const bool shouldDequeue = (isSequenceNumberUseful && isAckUseful) || isMaxTimeoutHit;

        scrooge::CrossChainMessageData newMessageData;
        if (shouldDequeue && messageInput->try_dequeue(newMessageData) && not is_test_over())
        {
            const auto sequenceNumber = newMessageData.sequence_number();

            startTimer(sequenceNumber, curTime);

            const auto resendNumber = messageScheduler.getResendNumber(sequenceNumber);
            const auto isMessageNeverSent = not resendNumber.has_value();
            if (isMessageNeverSent)
            {
                continue;
            }

            const auto isMessageAlreadyDelivered = curQuack >= sequenceNumber;
            if (not isMessageAlreadyDelivered)
            {
                auto destinations = messageScheduler.getMessageDestinations(sequenceNumber);

                bool isFirstSender = resendNumber == 0;
                const auto isPossiblySentLater = not isFirstSender || destinations.size() > 1;
                if (isFirstSender)
                {
                    const auto receiverNode = destinations.at(0);
                    numMessagesSent++;

                    bool wasSent{};
                    if (isPossiblySentLater)
                    {
                        scrooge::CrossChainMessageData messageDataCopy;
                        messageDataCopy.CopyFrom(newMessageData);
                        wasSent = pipeline->SendToOtherRsm(receiverNode, std::move(messageDataCopy), acknowledgment.get());
                    }
                    else
                    {
                        wasSent = pipeline->SendToOtherRsm(receiverNode, std::move(newMessageData), acknowledgment.get());
                    }

                    if (wasSent)
                    {
                        lastMessageSendTime = curTime;
                        numMsgsSentWithLastAck++;
                    }
                }

                if (isPossiblySentLater)
                {
                    const uint64_t numDestinationsAlreadySent = isFirstSender;

                    auto resendData = iothread::MessageResendData{.sequenceNumber = sequenceNumber,
                                                                  .firstDestinationResendNumber = resendNumber.value(),
                                                                  .numDestinationsSent = numDestinationsAlreadySent,
                                                                  .messageData = std::move(newMessageData),
                                                                  .destinations = std::move(destinations)};
                    
                    while (not resendMsgQueue.try_enqueue(std::move(resendData)) && not is_test_over());
                }
            }
        }
        else if ((isNoopTimeoutHit && isAckUseful) || isMaxTimeoutHit)
        {
            static uint64_t receiver = 0;
            const auto curAck = acknowledgment->getAckIterator();
            
            bool wasSent = pipeline->SendToOtherRsm(receiver % kOtherNetworkSize, {}, acknowledgment.get());

            assert(wasSent);
            receiver++;
            noop_ack++;

            numMsgsSentWithLastAck++;
            lastMessageSendTime = curTime;
            lastNoopTime = curTime;
        }
    }

    lameResendThread.join();

    addMetric("Num Floods", numMsgsFlushed);
    addMetric("Noop Acks", noop_ack);
    addMetric("Noop Delay",std::chrono::duration<double>(kNoopDelay).count());
    addMetric("Ack Window Size", kAckWindowSize);
    addMetric("Quack Window Size", kQAckWindowSize);
    addMetric("Latency", averageLat());
    addMetric("transfer_strategy", "Scrooge k=" + std::to_string(kListSize) + "+ resends");
    addMetric("num_msgs_sent_primary", numMessagesSent);
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
uint64_t numMissingUnder512 = 0;
template<uint64_t kListSize>
void updateAckTrackers(const std::optional<uint64_t> curQuack,
                       const uint64_t nodeId,
                       const uint64_t nodeStake,
                       const acknowledgment::AckView<kListSize> nodeAckView,
                       std::vector<std::unique_ptr<AcknowledgmentTracker>>* const ackTrackers)
{
    numRecv++;
    const auto kNumAckTrackers = ackTrackers->size();
    const auto initialMessageTrack = curQuack.value_or(0ULL - 1ULL) + 1;
    const auto finialMessageTrack = 
        std::min(initialMessageTrack + kNumAckTrackers - 1, acknowledgment::getFinalAck(nodeAckView));
    for (uint64_t curMessage = initialMessageTrack; curMessage <= finialMessageTrack; curMessage++)
    {
        numChecked++;
        const auto virtualQuack = (curMessage)? std::optional<uint64_t>(curMessage-1) : std::nullopt;
        const auto isNodeMissingCurMessage = not acknowledgment::testAckView(nodeAckView, curMessage);
        const auto curAckTracker = ackTrackers->data() + (curMessage % ackTrackers->size());

        if (isNodeMissingCurMessage)
        {
            numMissing++;
            numMissingUnder512 += curMessage < initialMessageTrack + 256;
            (*curAckTracker)->update(nodeId, nodeStake, virtualQuack, virtualQuack);
        }
        else
        {
            (*curAckTracker)->update(nodeId, nodeStake, virtualQuack.value_or(0ULL - 1ULL) + 1, virtualQuack);
        }
    }
}

struct LameAckData{
    uint64_t senderId{};
    acknowledgment::AckView<kListSize> senderAckView;
};
void lameAckThread(
    QuorumAcknowledgment* const quorumAck,
    std::vector<std::unique_ptr<AcknowledgmentTracker>>* const ackTrackers,
    moodycamel::BlockingReaderWriterCircularBuffer<LameAckData>* const viewQueue,
    const NodeConfiguration configuration)
{
    bindThreadToCpu(1);
    SPDLOG_CRITICAL("STARTING LAME THREAD {}", gettid());

    while (not is_test_over())
    {
        LameAckData curData{};
        while (not viewQueue->wait_dequeue_timed(curData, 1s) && not is_test_over());

        const auto& [senderId, senderAckView] = curData;

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
    // bindThreadToCpu(3);
    SPDLOG_CRITICAL("RECV THREAD TID {}", gettid());

    uint64_t numForeignMsgsRecv{}, numForeignBatchesRecv{}, numLocalMsgsRecv{}, numLocalBatchesRecv{};
    pipeline::ReceivedCrossChainMessage receivedMessage{};

    scrooge::CrossChainMessage crossChainMessage;
    auto ackView = std::array<uint64_t, acknowledgment::AckView<kListSize>::kNumInts>{};

    moodycamel::BlockingReaderWriterCircularBuffer<LameAckData> viewQueue(1<<20);
    std::thread lameThread(lameAckThread, quorumAck.get(), ackTrackers.get(), &viewQueue, configuration);

    while (not is_test_over())
    {
        if (receivedMessage.message == nullptr)
        {
            receivedMessage = pipeline->RecvFromOtherRsm();

            if (receivedMessage.message != nullptr)
            {
                numForeignBatchesRecv++;

                const auto [message, senderId] = receivedMessage;
                
                const auto messageData = nng_msg_body(message);
                const auto messageSize = nng_msg_len(message);
                bool success = crossChainMessage.ParseFromArray(messageData, messageSize);
                if (not success)
                {
                    SPDLOG_CRITICAL("Cannot parse foreign message");
                }

                const auto curForeignAck = (crossChainMessage.has_ack_count())
                                                ? std::optional<uint64_t>(crossChainMessage.ack_count().value())
                                                : std::nullopt;

                for (const auto& messageData : crossChainMessage.data())
                {
                    if (not isMessageDataValid(messageData))
                    {
                        continue;
                    }
                    acknowledgment->addToAckList(messageData.sequence_number());
                    numForeignMsgsRecv++;
                }

                assert(ackView.size() == crossChainMessage.ack_set_size() && "AckListSize inconsistent");
                std::copy_n(
                    crossChainMessage.ack_set().begin(),
                    ackView.size(),
                    ackView.begin()
                );
                const auto senderAckView = acknowledgment::AckView<kListSize>{
                    .ackOffset = curForeignAck.value_or(0ULL - 1ULL) + 2,
                    .view = ackView
                };

                while (
                    not viewQueue.wait_enqueue_timed({.senderId = senderId, .senderAckView = senderAckView}, 1s)
                    && not is_test_over()
                );

                bool isEmptyMessage = crossChainMessage.data_size() == 0;
                if (isEmptyMessage)
                {
                    nng_msg_free(receivedMessage.message);
                    receivedMessage.message = nullptr;
                }
                else
                {
                    crossChainMessage.clear_ack_count();
                    crossChainMessage.clear_ack_set();
                    const auto resultantSize = crossChainMessage.ByteSizeLong();
                    bool success = crossChainMessage.SerializeToArray(messageData, messageSize);
                    nng_msg_chop(message, (int64_t) messageSize - (int64_t) resultantSize);
                    if (not success)
                    {
                        SPDLOG_CRITICAL("Cannot Serialize foreign message");
                    }
                }
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
            numLocalBatchesRecv++;
            const auto messageData = nng_msg_body(message);
            const auto messageSize = nng_msg_len(message);
            bool success = crossChainMessage.ParseFromArray(messageData, messageSize);
            nng_msg_free(message);
            if (not success)
            {
                SPDLOG_CRITICAL("Cannot parse local message");
            }

            for (const auto& messageData : crossChainMessage.data())
            {
                if (not isMessageDataValid(messageData))
                {
                    continue;
                }
                acknowledgment->addToAckList(messageData.sequence_number());
                numLocalMsgsRecv++;
            }
        }
    }

    if (receivedMessage.message)
    {
        nng_msg_free(receivedMessage.message);
    }

    lameThread.join();

    SPDLOG_INFO("ALL MESSAGES RECEIVED : Receive thread exiting");
    addMetric("Average Missing Acks", (double)numMissing /(double) numChecked);
    addMetric("Average KList Size", (double)numChecked / (double) numRecv);
    addMetric("Average Missing Acks le512", (double) numMissingUnder512 / (double) numChecked);
    addMetric("NumKListRecv", numRecv);
    addMetric("local_messages_received", numLocalMsgsRecv);
    addMetric("foreign_messages_received", numForeignMsgsRecv);
    addMetric("local_batches_received", numLocalBatchesRecv);
    addMetric("foreign_batches_received", numForeignBatchesRecv);
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
    // bindThreadToCpu(1);
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

        for (const auto& messageData : crossChainMessage.data())
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
