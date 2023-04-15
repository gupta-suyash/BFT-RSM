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

#include <boost/circular_buffer.hpp>
#include <nng/nng.h>

void generateMessageMac(scrooge::CrossChainMessage *const message)
{
    // MAC signing TODO
    const auto mstr = message->ack_count().SerializeAsString();
    // Sign with own key.
    std::string encoded = CmacSignString(get_priv_key(), mstr);
    message->set_validity_proof(encoded);
    // std::cout << "Mac: " << encoded << std::endl;
}

bool checkMessageMac(const scrooge::CrossChainMessage *const message)
{
    // Verification TODO
    const auto mstr = message->ack_count().SerializeAsString();
    // Fetch the sender key
    // const auto senderKey = get_other_rsm_key(nng_message.senderId);
    const auto senderKey = get_priv_key();
    // Verify the message
    bool res = CmacVerifyString(senderKey, mstr, message->validity_proof());
    return res;
}

void setAckValue(scrooge::CrossChainMessage *const message, const Acknowledgment &acknowledgment)
{
    const auto curAck = acknowledgment.getAckIterator();
    if (!curAck.has_value())
    {
        return;
    }

    message->mutable_ack_count()->set_value(curAck.value());
}

bool isMessageValid(const scrooge::CrossChainMessage &message)
{
    // no signature checking currently
    return message.has_data();
}

// Generates fake messages of a given size for throughput testing
void runGenerateMessageThread(const std::shared_ptr<iothread::MessageQueue> messageOutput,
                              const NodeConfiguration configuration)
{
    const auto kMessageSize = get_packet_size();

    for (uint64_t curSequenceNumber = 0; not is_test_over(); curSequenceNumber++)
    {
        scrooge::CrossChainMessage fakeMessage;

        scrooge::CrossChainMessageData *const fakeData = fakeMessage.mutable_data();
        fakeData->set_message_content(std::string(kMessageSize, 'L'));
        fakeData->set_sequence_number(curSequenceNumber);

        //fakeMessage.set_validity_proof(std::string(kMessageSize / 2, 'X'));

        while (not messageOutput->wait_enqueue_timed(std::move(fakeMessage), 100ms) && not is_test_over())
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
    //for (uint64_t curSequenceNumber = 0; not is_test_over(); curSequenceNumber++)
    while (not is_test_over())
    {
	while (std::chrono::steady_clock::now() - lastSendTime < kMessageSpace);
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
                const auto newMessageRequest = newRequest.send_message_request();

                scrooge::CrossChainMessage newMessage;
                *newMessage.mutable_data() = newMessageRequest.content();

                *newMessage.mutable_validity_proof() = newMessageRequest.validity_proof();

                receivedMessages.addToAckList(newMessage.data().sequence_number());
                numReceivedMessages++;

                while (not messageOutput->wait_enqueue_timed(std::move(newMessage), 1ms) && not is_test_over())
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
                           const std::shared_ptr<AcknowledgmentTracker> ackTracker,
                           const std::shared_ptr<QuorumAcknowledgment> quorumAck, const NodeConfiguration configuration)
{
    //bindThreadToCpu(0);
    SPDLOG_INFO("Send Thread starting with TID = {}", gettid());

    uint64_t numMessagesSent{};
    Acknowledgment sentMessages{};
    while (not is_test_over())
    {
        scrooge::CrossChainMessage newMessage;
        while (messageInput->try_dequeue(newMessage) && not is_test_over())
        {
            const auto curSequenceNumber = newMessage.data().sequence_number();
	    auto start_time = std::chrono::high_resolution_clock::now();

            pipeline->SendToAllOtherRsm(newMessage);
            sentMessages.addToAckList(curSequenceNumber);
            quorumAck->updateNodeAck(0, 0ULL - 1, sentMessages.getAckIterator().value_or(0));
            numMessagesSent++;

	    allToall(start_time);
        }
    }

    addMetric("Latency", averageLat());
    addMetric("transfer_strategy", "NSendNRecv Thread All-to-All");
    addMetric("num_msgs_sent", numMessagesSent);
    //addMetric("max_quack", quorumAck->getCurrentQuack().value_or(0));
    SPDLOG_INFO("ALL CROSS CONSENSUS PACKETS SENT : send thread exiting");
}

void runOneToOneSendThread(const std::shared_ptr<iothread::MessageQueue> messageInput,
                           const std::shared_ptr<Pipeline> pipeline,
                           const std::shared_ptr<Acknowledgment> acknowledgment,
                           const std::shared_ptr<AcknowledgmentTracker> ackTracker,
                           const std::shared_ptr<QuorumAcknowledgment> quorumAck, const NodeConfiguration configuration)
{
    //bindThreadToCpu(0);
    SPDLOG_INFO("Send Thread starting with TID = {}", gettid());
    const auto &[kOwnNetworkSize, kOtherNetworkSize, kOwnNetworkStakes, kOtherNetworkStakes, kOwnMaxNumFailedStake,
                 kOtherMaxNumFailedStake, kNodeId, kLogPath, kWorkingDir] = configuration;

    uint64_t numMessagesSent{};
    Acknowledgment sentMessages{};
    while (not is_test_over())
    {
        scrooge::CrossChainMessage newMessage;
        while (messageInput->try_dequeue(newMessage) && not is_test_over())
        {
            const auto curSequenceNumber = newMessage.data().sequence_number();
	    auto start_time = std::chrono::high_resolution_clock::now();

            pipeline->SendToOtherRsm((kNodeId + curSequenceNumber) % kOtherNetworkSize, newMessage);
	    sentMessages.addToAckList(curSequenceNumber);
	    quorumAck->updateNodeAck(0, 0ULL - 1, sentMessages.getAckIterator().value_or(0));
            numMessagesSent++;

	    allToall(start_time);
        }
    }

    addMetric("Latency", averageLat());
    addMetric("transfer_strategy", "NSendNRecv Thread One-to-One");
    addMetric("num_msgs_sent", numMessagesSent);
    SPDLOG_INFO("ALL CROSS CONSENSUS PACKETS SENT : send thread exiting");
}

void runSendThread(const std::shared_ptr<iothread::MessageQueue> messageInput, 
		   const std::shared_ptr<Pipeline> pipeline,
                   const std::shared_ptr<Acknowledgment> acknowledgment,
                   const std::shared_ptr<AcknowledgmentTracker> ackTracker,
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

    //bindThreadToCpu(0);
    SPDLOG_INFO("Send Thread starting with TID = {}", gettid());

    const MessageScheduler messageScheduler(configuration);

    uint64_t numMessagesResent{};
    auto resendMessageMap = std::map<uint64_t, iothread::MessageResendData>{};
    auto lastSendTime = std::chrono::steady_clock::now();
    auto lastNoopTime = std::chrono::steady_clock::now();
    uint64_t numMsgsSentWithLastAck{};
    std::optional<uint64_t> lastSentAck{};
    uint64_t lastQuack = 0, lastSeq = 0;
    constexpr uint64_t kAckWindowSize = 100'000'000;
    constexpr uint64_t kQAckWindowSize = 100'000'000; 
    // Optimal window size for non-stake: 12*16 and for stake: 12*8
    constexpr auto kMaxMessageDelay = 0ms;
    constexpr auto kNoopDelay = 5000s;
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

        const int64_t pendingSequenceNum = (messageInput->peek()) ? messageInput->peek()->data().sequence_number()
                                                                  : kQAckWindowSize + curQuack.value_or(0);
        const bool isAckFresh = numMsgsSentWithLastAck < kAckWindowSize;
        const bool isSequenceNumberUseful = pendingSequenceNum - curQuack.value_or(0ULL - 1) < kQAckWindowSize;
	const auto curTime = std::chrono::steady_clock::now();
	const bool isNoopTimeoutHit = curTime - std::max(lastNoopTime,lastSendTime) > kNoopDelay;
        const bool isTimeoutHit = curTime - lastSendTime > kMaxMessageDelay;
        const bool shouldDequeue = isTimeoutHit || (isAckFresh && isSequenceNumberUseful);

        scrooge::CrossChainMessage newMessage;
        if (shouldDequeue && messageInput->try_dequeue(newMessage) && not is_test_over())
        {
            const auto sequenceNumber = newMessage.data().sequence_number();
	    if (lastSeq != sequenceNumber) {
		SPDLOG_CRITICAL("SeqFail: L:{} :: N:{}", lastSeq,sequenceNumber);
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
            if (isFirstSender)
            {
                const auto receiverNode = destinations.at(0);

                setAckValue(&newMessage, *acknowledgment);
                generateMessageMac(&newMessage);
                pipeline->SendToOtherRsm(receiverNode, newMessage);
                lastSendTime = std::chrono::steady_clock::now();
                numMsgsSentWithLastAck++;
            }

            const auto isPossiblySentLater = not isFirstSender || destinations.size() > 1;
            if (isPossiblySentLater)
            {
                const uint64_t numDestinationsAlreadySent = isFirstSender;
                resendMessageMap.emplace(
                    sequenceNumber, iothread::MessageResendData{.message = std::move(newMessage),
                                                                .firstDestinationResendNumber = resendNumber.value(),
                                                                .numDestinationsSent = numDestinationsAlreadySent,
                                                                .destinations = destinations});
            }
        } 
	else if(isAckFresh && isNoopTimeoutHit) 
	{
	    static uint64_t receiver = 0;
	    setAckValue(&newMessage, *acknowledgment);
            generateMessageMac(&newMessage);
            pipeline->SendToOtherRsm(receiver % kOtherNetworkSize, newMessage);
	    receiver++;
            numMsgsSentWithLastAck++; 
	    noop_ack++;

	    lastNoopTime = curTime;
	}	

        const auto curQuorumAck = quorumAck->getCurrentQuack();
        const auto activeResendData = ackTracker->getActiveResendData();
        for (auto it = resendMessageMap.begin(); it != resendMessageMap.end() && not is_test_over();)
        {
            const auto sequenceNumber = it->first;
            auto &[message, firstDestinationResendNumber, numDestinationsSent, destinations] = it->second;

            const bool isMessageAlreadyDelivered = sequenceNumber <= curQuorumAck;
            if (isMessageAlreadyDelivered)
            {
                it = resendMessageMap.erase(it);
                continue;
            }

            const bool isNoMessageToResend =
                not activeResendData.has_value() || activeResendData->sequenceNumber < sequenceNumber;
            if (isNoMessageToResend)
            {
                // No message to resend, or missing a message to resend
                break;
            }
            else if (activeResendData->sequenceNumber > sequenceNumber)
            {
                // Message to resend may be further in the map
                it++;
                continue;
            }

            // We got to the message which should be resent
            const auto curNodeFirstResend = firstDestinationResendNumber;
            const auto curNodeLastResend = firstDestinationResendNumber + destinations.size() - 1;
            const auto curNodeCompletedResends = curNodeFirstResend + numDestinationsSent;
            const auto curFinalDestination = std::min<uint64_t>(curNodeLastResend, activeResendData->resendNumber);
            for (uint64_t resend = curNodeCompletedResends; resend <= curFinalDestination; resend++)
            {
                const auto destination = destinations.at(resend - curNodeFirstResend);
                setAckValue(&message, *acknowledgment);
                generateMessageMac(&message);
                pipeline->SendToOtherRsm(destination, message);
                lastSendTime = std::chrono::steady_clock::now();
                numMsgsSentWithLastAck++;
                numDestinationsSent++;
                numMessagesResent += is_test_recording();
            }

            const auto isComplete = numDestinationsSent == destinations.size();
            if (isComplete)
            {
                resendMessageMap.erase(it);
            }
            break;
        }
    }

    addMetric("Noop Acks", noop_ack);
    addMetric("Latency", averageLat());
    addMetric("transfer_strategy", "NSendNRecv Thread Scrooge");
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
	        //SPDLOG_CRITICAL("Write: {} :: N:{} :: R:{}",lastQuorumAck.value(), kNodeConfiguration.kNodeId, get_rsm_id()); 
            writeMessage(pipe, serializedTransfer);
        }
    }
    pipe.close();
    addMetric("IPC test",true);
}

static void runLocalReceiveThread(const std::shared_ptr<Pipeline> pipeline,
                                  const std::shared_ptr<Acknowledgment> acknowledgment)
{
    SPDLOG_CRITICAL("LOCAL RECV THREAD TID {}", gettid());
    uint64_t timedMessages{};
    scrooge::CrossChainMessage message;
    while (not is_test_over() && not is_test_over())
    {
        const auto [batchMessage, senderId] = pipeline->RecvFromOwnRsm();
        if (not batchMessage)
        {
            std::this_thread::yield();
            continue;
        }
        const auto batchData = (char*) nng_msg_body(batchMessage);
        const auto batchSize = nng_msg_len(batchMessage);
        uint64_t batchPos = 0;

        while (batchPos < batchSize)
        {
            constexpr auto delimiterSize = sizeof(uint32_t);
            const auto delimiterPos = batchData + batchPos;
            const auto messagePos = batchData + batchPos + delimiterSize;
            const auto messageSize = *(reinterpret_cast<uint32_t*>(delimiterPos));

            message.Clear();
            message.ParseFromArray(messagePos, messageSize);
            batchPos += delimiterSize + messageSize;

            if (isMessageValid(message))
            {
                timedMessages += is_test_recording();
                acknowledgment->addToAckList(message.data().sequence_number());
            }
        }
        nng_msg_free(batchMessage);
    }
    addMetric("local_messages_received", timedMessages);
}

void runReceiveThread(const std::shared_ptr<Pipeline> pipeline, const std::shared_ptr<Acknowledgment> acknowledgment,
                      const std::shared_ptr<AcknowledgmentTracker> ackTracker,
                      const std::shared_ptr<QuorumAcknowledgment> quorumAck, const NodeConfiguration configuration)
{
    SPDLOG_CRITICAL("FOREIGN RECV THREAD TID {}", gettid());
    const auto &[kOwnNetworkSize, kOtherNetworkSize, kOwnNetworkStakes, kOtherNetworkStakes, kOwnMaxNumFailedStake,
                 kOtherMaxNumFailedStake, kNodeId, kLogPath, kWorkingDir] = configuration;
    auto localReceiveThread = std::thread(runLocalReceiveThread, pipeline, acknowledgment);

    uint64_t timedMessages{};
    scrooge::CrossChainMessage foreignMessage;

    while (not is_test_over())
    {
        const auto [batchMessage, senderId] = pipeline->RecvFromOtherRsm();
        if (not batchMessage)
        {
            std::this_thread::yield();
            continue;
        }
        const auto batchData = (char*) nng_msg_body(batchMessage);
        const auto batchSize = nng_msg_len(batchMessage);
        uint64_t batchPos = 0;

        while (batchPos < batchSize && not is_test_over())
        {
            constexpr auto delimiterSize = sizeof(uint32_t);
            const auto delimiterPos = batchData + batchPos;
            const auto messagePos = batchData + batchPos + delimiterSize;
            const auto messageSize = *(reinterpret_cast<uint32_t*>(delimiterPos));

            foreignMessage.Clear();
            foreignMessage.ParseFromArray(messagePos, messageSize);
            batchPos += delimiterSize + messageSize;

            if (isMessageValid(foreignMessage))
            {
                acknowledgment->addToAckList(foreignMessage.data().sequence_number());
                timedMessages += is_test_recording();
            }

            if (foreignMessage.has_ack_count())
            {
                bool isAckValid = checkMessageMac(&foreignMessage);
                if (not isAckValid)
                {
                    SPDLOG_CRITICAL("CANNOT VERIFY ACK OF MESSAGE {}", foreignMessage.data().sequence_number());
                    continue;
                }
                const auto foreignAckCount = foreignMessage.ack_count().value();
                const auto senderStake = kOtherNetworkStakes.at(senderId);
                const auto currentQuack = quorumAck->updateNodeAck(senderId, senderStake, foreignAckCount);
                ackTracker->update(senderId, senderStake, foreignAckCount, currentQuack);
            }
            else
            {
                const auto foreignAckCount = std::nullopt;
                const auto senderStake = kOtherNetworkStakes.at(senderId);
                const auto currentQuack = quorumAck->getCurrentQuack();
                ackTracker->update(senderId, senderStake, foreignAckCount, currentQuack);
            }
        }
        pipeline->rebroadcastToOwnRsm(batchMessage);
    }
    SPDLOG_INFO("ALL MESSAGES RECEIVED : Receive thread exiting");
    localReceiveThread.join();
    addMetric("foreign_messages_received", timedMessages);
    addMetric("max_acknowledgment", acknowledgment->getAckIterator().value_or(0));
    addMetric("max_quorum_acknowledgment", quorumAck->getCurrentQuack().value_or(0));
}

void runAllToAllReceiveThread(const std::shared_ptr<Pipeline> pipeline,
                              const std::shared_ptr<Acknowledgment> acknowledgment,
                              const std::shared_ptr<AcknowledgmentTracker> ackTracker,
                              const std::shared_ptr<QuorumAcknowledgment> quorumAck,
                              const NodeConfiguration configuration)
{
    //bindThreadToCpu(1);
    uint64_t timedMessages{};
    scrooge::CrossChainMessage message;

    while (not is_test_over())
    {
        const auto [batchMessage, senderId] = pipeline->RecvFromOtherRsm();
        if (not batchMessage)
        {
            std::this_thread::yield();
            continue;
        }
        const auto batchData = (char*) nng_msg_body(batchMessage);
        const auto batchSize = nng_msg_len(batchMessage);
        uint64_t batchPos = 0;

        while (batchPos < batchSize && not is_test_over())
        {
            constexpr auto delimiterSize = sizeof(uint32_t);
            const auto delimiterPos = batchData + batchPos;
            const auto messagePos = batchData + batchPos + delimiterSize;
            const auto messageSize = *(reinterpret_cast<uint32_t*>(delimiterPos));

            message.Clear();
            message.ParseFromArray(messagePos, messageSize);
            batchPos += delimiterSize + messageSize;
            if (isMessageValid(message))
            {
                timedMessages += is_test_recording();
                acknowledgment->addToAckList(message.data().sequence_number());
            }
        }
    }

    addMetric("local_messages_received", 0);
    addMetric("foreign_messages_received", timedMessages);
    addMetric("max_acknowledgment", acknowledgment->getAckIterator().value_or(0));
    addMetric("max_quorum_acknowledgment", quorumAck->getCurrentQuack().value_or(0));
}
