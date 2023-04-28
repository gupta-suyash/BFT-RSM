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
            auto start_time = std::chrono::high_resolution_clock::now();

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
            auto start_time = std::chrono::high_resolution_clock::now();

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

void runSendThread(const std::shared_ptr<iothread::MessageQueue> messageInput, const std::shared_ptr<Pipeline> pipeline,
                   const std::shared_ptr<Acknowledgment> acknowledgment,
                   const std::shared_ptr<std::vector<std::unique_ptr<AcknowledgmentTracker>>> ackTrackers,
                   const std::shared_ptr<QuorumAcknowledgment> quorumAck, const NodeConfiguration configuration)
{
    SPDLOG_CRITICAL("SEND THREAD TID {}", gettid());
    const auto &[kOwnNetworkSize, kOtherNetworkSize, kOwnNetworkStakes, kOtherNetworkStakes, kOwnMaxNumFailedStake,
                 kOtherMaxNumFailedStake, kNodeId, kLogPath, kWorkingDir] = configuration;

    const bool isFirstNode = configuration.kNodeId == 0 && get_rsm_id() == 0;
    if (isFirstNode)
    {
        SPDLOG_CRITICAL("Send TID == {}", gettid());
    }

    // bindThreadToCpu(0);
    SPDLOG_INFO("Send Thread starting with TID = {}", gettid());

    const MessageScheduler messageScheduler(configuration);

    uint64_t numMessagesResent{};
    auto resendMessageMap = std::map<uint64_t, iothread::MessageResendData>{};
    auto lastSendTime = std::chrono::steady_clock::now();
    auto lastNoopTime = std::chrono::steady_clock::now();
    uint64_t numMsgsSentWithLastAck{};
    std::optional<uint64_t> lastSentAck{};
    uint64_t lastQuack = 0, lastSeq = 0;
    constexpr uint64_t kAckWindowSize = 12*16*10;
    constexpr uint64_t kQAckWindowSize = 256;
    // Optimal window size for non-stake: 12*16 and for stake: 12*8
    constexpr auto kMaxMessageDelay = 1us;
    constexpr auto kNoopDelay = 500us;
    constexpr auto kAckTrackerTimeout = 1ms;
    std::chrono::steady_clock::time_point lastAckTrackerCheck{};
    // kNoopDelay for Scrooge for non-failures: 500
    uint64_t noop_ack = 0;

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

        const int64_t pendingSequenceNum = (messageInput->peek()) ? messageInput->peek()->sequence_number()
                                                                  : kQAckWindowSize + curQuack.value_or(0);
        const bool isAckFresh = numMsgsSentWithLastAck < kAckWindowSize;
        const bool isSequenceNumberUseful = pendingSequenceNum - curQuack.value_or(0ULL - 1) < kQAckWindowSize;
        const auto curTime = std::chrono::steady_clock::now();
        const bool isNoopTimeoutHit = curTime - std::max(lastNoopTime, lastSendTime) > kNoopDelay;
        const bool isTimeoutHit = curTime - lastSendTime > kMaxMessageDelay;
        const bool shouldDequeue = isTimeoutHit || (isAckFresh && isSequenceNumberUseful);

        scrooge::CrossChainMessageData newMessageData;
        if (shouldDequeue && messageInput->try_dequeue(newMessageData) && not is_test_over())
        {
            const auto sequenceNumber = newMessageData.sequence_number();
            if (lastSeq != sequenceNumber)
            {
                SPDLOG_CRITICAL("SeqFail: L:{} :: N:{}", lastSeq, sequenceNumber);
            }
            lastSeq++;

            // Start the timer for latency
            startTimer(sequenceNumber);

            // Recording latency
            uint64_t checkVal = curQuack.value_or(0);
            recordLatency(lastQuack, checkVal);
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

                if (isPossiblySentLater)
                {
                    auto messageDataCopy = newMessageData;
                    pipeline->SendToOtherRsm(receiverNode, std::move(messageDataCopy), acknowledgment.get());
                }
                else
                {
                    pipeline->SendToOtherRsm(receiverNode, std::move(newMessageData), acknowledgment.get());
                }
                lastSendTime = std::chrono::steady_clock::now();
                numMsgsSentWithLastAck++;
            }

            if (isPossiblySentLater)
            {
                const uint64_t numDestinationsAlreadySent = isFirstSender;
                resendMessageMap.emplace(
                    sequenceNumber, iothread::MessageResendData{.messageData = std::move(newMessageData),
                                                                .firstDestinationResendNumber = resendNumber.value(),
                                                                .numDestinationsSent = numDestinationsAlreadySent,
                                                                .destinations = destinations});
            }
        }
        else if (isAckFresh && isNoopTimeoutHit)
        {
            static uint64_t receiver = 0;
            
            pipeline->SendToOtherRsm(receiver % kOtherNetworkSize, {}, acknowledgment.get());
            receiver++;
            numMsgsSentWithLastAck++;
            noop_ack++;

            lastNoopTime = curTime;
        }

        const auto curQuorumAck = quorumAck->getCurrentQuack();

        // Prune old message resend data
        for (auto it = resendMessageMap.begin(); it != resendMessageMap.end() && not is_test_over();)
        {
            const auto sequenceNumber = it->first;

            const bool isMessageAlreadyDelivered = sequenceNumber <= curQuorumAck;
            if (isMessageAlreadyDelivered)
            {
                it = resendMessageMap.erase(it);
            }
            else
            {
                break;
            }
        }

        bool shouldSkipAckTrackCheck = curTime - lastAckTrackerCheck <= kAckTrackerTimeout;
        if (shouldSkipAckTrackCheck)
        {
            continue;
        }
        lastAckTrackerCheck = curTime;
        for (const auto& ackTracker : *ackTrackers)
        {
            const auto activeResendData = ackTracker->getActiveResendData();
            if (not activeResendData.has_value())
            {
                continue;
            }
            const auto [resendSequenceNumber, resendNumber] = *activeResendData;
            auto resendMessageData = resendMessageMap.find(resendSequenceNumber);

            if (resendMessageData != resendMessageMap.end())
            {
                auto &[message, firstDestinationResendNumber, numDestinationsSent, destinations] = resendMessageData->second;
                // We got to the message which should be resent
                const auto curNodeFirstResend = firstDestinationResendNumber;
                const auto curNodeLastResend = firstDestinationResendNumber + destinations.size() - 1;
                const auto curNodeCompletedResends = curNodeFirstResend + numDestinationsSent;
                const auto curFinalDestination = std::min<uint64_t>(curNodeLastResend, activeResendData->resendNumber);
                for (uint64_t resend = curNodeCompletedResends; resend <= curFinalDestination; resend++)
                {
                    const auto destination = destinations.at(resend - curNodeFirstResend);

                    bool wasSent{};
                    const bool isSentLater = numDestinationsSent + 1 < destinations.size();
                    if (isSentLater)
                    {
                        auto messageDataCopy = message;
                        wasSent = pipeline->SendToOtherRsm(destination, std::move(messageDataCopy), acknowledgment.get());
                    }
                    else
                    {
                        wasSent = pipeline->SendToOtherRsm(destination, std::move(message), acknowledgment.get());
                    }
                    
                    if (not wasSent)
                    {
                        // Expedite sending of failed message
                        // Check if this helps or hurts performance @Reggie
                        pipeline->SendToOtherRsm(destination, {}, acknowledgment.get());
                    }

                    lastSendTime = std::chrono::steady_clock::now();
                    numMsgsSentWithLastAck++;
                    numDestinationsSent++;
                    numMessagesResent += is_test_recording();
                }

                const auto isComplete = numDestinationsSent == destinations.size();
                if (isComplete)
                {
                    resendMessageMap.erase(resendMessageData);
                }
            }
        }
    }

    addMetric("Noop Acks", noop_ack);
    addMetric("Noop Delay",std::chrono::duration<double>(kNoopDelay).count());
    addMetric("Ack Window",std::chrono::duration<double>(kAckWindowSize).count());
    addMetric("Quack Window",std::chrono::duration<double>(kQAckWindowSize).count());
    addMetric("Latency", averageLat());
    addMetric("transfer_strategy", "Scrooge k=" + std::to_string(kListSize) + "+ resends");
    addMetric("resend_msg_map_size", resendMessageMap.size());
    addMetric("num_msgs_resent", numMessagesResent);
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
            (*curAckTracker)->update(nodeId, nodeStake, virtualQuack, virtualQuack);
        }
        else
        {
            (*curAckTracker)->update(nodeId, nodeStake, virtualQuack.value_or(0ULL - 1ULL) + 1, virtualQuack);
        }
    }
}

void runReceiveThread(const std::shared_ptr<Pipeline> pipeline, const std::shared_ptr<Acknowledgment> acknowledgment,
                      const std::shared_ptr<std::vector<std::unique_ptr<AcknowledgmentTracker>>> ackTrackers,
                      const std::shared_ptr<QuorumAcknowledgment> quorumAck, const NodeConfiguration configuration)
{
    SPDLOG_CRITICAL("RECV THREAD TID {}", gettid());

    uint64_t timedMessages{};
    pipeline::ReceivedCrossChainMessage receivedMessage{};

    scrooge::CrossChainMessage crossChainMessage;
    auto ackView = std::array<uint64_t, acknowledgment::AckView<kListSize>::kNumInts>{};

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

                for (const auto& messageData : crossChainMessage.data())
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
                const auto senderStake = configuration.kOtherNetworkStakes.at(senderId);
                if (curForeignAck.has_value())
                {
                    quorumAck->updateNodeAck(senderId, senderStake, *curForeignAck);
                }

                const auto currentQuack = quorumAck->getCurrentQuack();
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
                updateAckTrackers(currentQuack, senderId, senderStake, senderAckView, ackTrackers.get());
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
    }

    if (receivedMessage.message)
    {
        nng_msg_free(receivedMessage.message);
    }
    SPDLOG_INFO("ALL MESSAGES RECEIVED : Receive thread exiting");
    addMetric("Average Missing Acks", (double)numMissing /(double) numChecked);
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
