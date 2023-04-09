#include "iothread.h"

#include "ipc.h"
#include "scrooge_message.pb.h"
#include "scrooge_request.pb.h"
#include "scrooge_transfer.pb.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <limits>
#include <map>
#include <pthread.h>
#include <stdio.h>
#include <thread>

#include <boost/circular_buffer.hpp>


bool isMessageValid(const scrooge::CrossChainMessage &message)
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
        scrooge::CrossChainMessage fakeMessage;

        scrooge::CrossChainMessageData *const fakeData = fakeMessage.mutable_data();
        fakeData->set_message_content(std::string(kMessageSize / 2, 'L'));
        fakeData->set_sequence_number(curSequenceNumber);

        fakeMessage.set_validity_proof(std::string(kMessageSize / 2, 'X'));

        while(not messageOutput->wait_enqueue_timed(std::move(fakeMessage), 100ms) && not is_test_over());
    }
}

// Relays messages to be sent over ipc
void runRelayIPCRequestThread(const std::shared_ptr<iothread::MessageQueue> messageOutput)
{
    constexpr auto kScroogeInputPath = "/tmp/scrooge-input";
    const auto readMessages = std::make_shared<ipc::DataChannel>(10);
    const auto exitReader = std::make_shared<std::atomic_bool>();

    createPipe(kScroogeInputPath);
    auto reader = std::thread(startPipeReader, kScroogeInputPath, readMessages, exitReader);

    while (not is_test_over())
    {
        constexpr auto kPollPeriod = 50ns;

        std::vector<uint8_t> messageBytes;
        while (not readMessages->pop(messageBytes))
        {
            if (*exitReader)
            {
                break;
            }
            std::this_thread::sleep_for(kPollPeriod);
        }
        if (*exitReader)
        {
            break;
        }

        scrooge::ScroogeRequest newRequest;

        const auto isParseSuccessful = newRequest.ParseFromArray(messageBytes.data(), messageBytes.size());
        if (not isParseSuccessful)
        {
            SPDLOG_ERROR("FAILED TO READ MESSAGE");
            continue;
        }

        switch (newRequest.request_case())
        {
            using request = scrooge::ScroogeRequest::RequestCase;
            case request::kSendMessageRequest: {
                const auto newMessageRequest = newRequest.send_message_request();

		//std::cout << "Entered" << std::endl;

                scrooge::CrossChainMessage newMessage;
                *newMessage.mutable_data() = newMessageRequest.content();
                *newMessage.mutable_validity_proof() = newMessageRequest.validity_proof();

                while(not messageOutput->wait_enqueue_timed(std::move(newMessage), 100ms) && not is_test_over());
                break;
            }
            default: {
                SPDLOG_ERROR("UNKNOWN REQUEST TYPE {}", newRequest.request_case());
            }
        }

        std::this_thread::sleep_for(kPollPeriod);
    }
    SPDLOG_INFO("Relay IPC Message Thread Exiting");
    *exitReader = true;
    reader.join();
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

void runAllToAllSendThread(const std::shared_ptr<iothread::MessageQueue> messageInput,
                           const std::shared_ptr<Pipeline> pipeline,
                           const std::shared_ptr<Acknowledgment> acknowledgment,
                           const std::shared_ptr<AcknowledgmentTracker> ackTracker,
                           const std::shared_ptr<QuorumAcknowledgment> quorumAck, const NodeConfiguration configuration)
{
    SPDLOG_INFO("Send Thread starting with TID = {}", gettid());

    while (not is_test_over())
    {
        scrooge::CrossChainMessage newMessage;
        while (messageInput->try_dequeue(newMessage))
        {
            pipeline->SendToAllOtherRsm(newMessage);
        }
    }

    SPDLOG_INFO("ALL CROSS CONSENSUS PACKETS SENT : send thread exiting");
}

void runOneToOneSendThread(const std::shared_ptr<iothread::MessageQueue> messageInput,
                           const std::shared_ptr<Pipeline> pipeline,
                           const std::shared_ptr<Acknowledgment> acknowledgment,
                           const std::shared_ptr<AcknowledgmentTracker> ackTracker,
                           const std::shared_ptr<QuorumAcknowledgment> quorumAck, const NodeConfiguration configuration)
{
    SPDLOG_INFO("Send Thread starting with TID = {}", gettid());
    const auto &[kOwnNetworkSize, kOtherNetworkSize, kOwnNetworkStakes, kOtherNetworkStakes, kOwnMaxNumFailedStake,
                 kOtherMaxNumFailedStake, kNodeId, kLogPath, kWorkingDir] = configuration;

    while (not is_test_over())
    {
        scrooge::CrossChainMessage newMessage;
        while (messageInput->try_dequeue(newMessage))
        {
            const auto curSequenceNumber = newMessage.data().sequence_number();
            pipeline->SendToOtherRsm(curSequenceNumber % kOtherNetworkSize, newMessage);
        }
    }

    SPDLOG_INFO("ALL CROSS CONSENSUS PACKETS SENT : send thread exiting");
}

void runSendThread(const std::shared_ptr<iothread::MessageQueue> messageInput, const std::shared_ptr<Pipeline> pipeline,
                   const std::shared_ptr<Acknowledgment> acknowledgment,
                   const std::shared_ptr<AcknowledgmentTracker> ackTracker,
                   const std::shared_ptr<QuorumAcknowledgment> quorumAck, const NodeConfiguration configuration)
{
    const bool isFirstNode = configuration.kNodeId == 0 && get_rsm_id() == 0;
    if (isFirstNode)
    {
        SPDLOG_CRITICAL("Send TID == {}", gettid());
    }

    bindThreadToCpu(0);
    SPDLOG_INFO("Send Thread starting with TID = {}", gettid());

    const MessageScheduler messageScheduler(configuration);

    uint64_t numMessagesResent{};
    auto resendMessageMap = std::map<uint64_t, iothread::MessageResendData>{};
    auto lastSendTime = std::chrono::steady_clock::now();
    uint64_t numMsgsSentWithLastAck{};
    std::optional<uint64_t> lastSentAck{};
    constexpr uint64_t kAckWindowSize = 8;
    constexpr uint64_t kQAckWindowSize = 12 * 4;
    constexpr auto kMaxMessageDelay = 2ms;

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

        const int64_t pendingSequenceNum = (messageInput->peek())?
                                            messageInput->peek()->data().sequence_number() :
                                            kQAckWindowSize + curQuack.value_or(0);
        const bool isAckFresh = numMsgsSentWithLastAck < kAckWindowSize;
        const bool isSequenceNumberUseful = pendingSequenceNum - curQuack.value_or(0ULL - 1);
        const bool isTimeoutHit = std::chrono::steady_clock::now() - lastSendTime > kMaxMessageDelay;
        const bool shouldDequeue = isTimeoutHit || (isAckFresh && isSequenceNumberUseful);

        scrooge::CrossChainMessage newMessage;
        if (shouldDequeue && messageInput->try_dequeue(newMessage) && not is_test_over())
        {
            const auto sequenceNumber = newMessage.data().sequence_number();
	    if(sequenceNumber == 1) {
	    	std::cout << "Seq: " << sequenceNumber << std::endl;
	    }
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

    addMetric("resend_msg_map_size", resendMessageMap.size());
    addMetric("num_msgs_resent", numMessagesResent);
    SPDLOG_INFO("ALL CROSS CONSENSUS PACKETS SENT : send thread exiting");
}

void runRelayIPCTransactionThread(std::string scroogeOutputPipePath, std::shared_ptr<QuorumAcknowledgment> quorumAck)
{
    std::ofstream pipe{scroogeOutputPipePath, std::ios_base::binary};
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
	    std::cout << "Going to Write" << std::endl;
            writeMessage(pipe, serializedTransfer);
        }
        std::this_thread::sleep_for(100ms);
    }
}

static void runLocalReceiveThread(const std::shared_ptr<Pipeline> pipeline, const std::shared_ptr<Acknowledgment> acknowledgment)
{
    bindThreadToCpu(6);
    uint64_t timedMessages{};
    boost::circular_buffer<scrooge::CrossChainMessage> domesticMessages(256);
    while (not is_test_over())
    {
        domesticMessages.clear();
        pipeline->RecvFromOwnRsm(domesticMessages);
        for (auto newDomesticMessage : domesticMessages)
        {
            if (isMessageValid(newDomesticMessage))
            {
                timedMessages += is_test_recording();
                acknowledgment->addToAckList(newDomesticMessage.data().sequence_number());
            }
        }
    }
    addMetric("local_messages_received", timedMessages);
}

void runReceiveThread(const std::shared_ptr<Pipeline> pipeline, const std::shared_ptr<Acknowledgment> acknowledgment,
                      const std::shared_ptr<AcknowledgmentTracker> ackTracker,
                      const std::shared_ptr<QuorumAcknowledgment> quorumAck, const NodeConfiguration configuration)
{
    const auto &[kOwnNetworkSize, kOtherNetworkSize, kOwnNetworkStakes, kOtherNetworkStakes, kOwnMaxNumFailedStake,
                 kOtherMaxNumFailedStake, kNodeId, kLogPath, kWorkingDir] = configuration;
    auto localReceiveThread = std::thread(runLocalReceiveThread, pipeline, acknowledgment);
    bindThreadToCpu(2);
    const bool isFirstNode = kNodeId == 0 && get_rsm_id() == 0;
    if (isFirstNode)
    {
        SPDLOG_CRITICAL("Receive TID == {}", gettid());
    }

    uint64_t timedMessages{};

    boost::circular_buffer<pipeline::ReceivedCrossChainMessage> foreignMessages(2048);

    while (not is_test_over())
    {
        foreignMessages.clear();
        pipeline->RecvFromOtherRsm(foreignMessages);

        for (auto& newForeignMessage : foreignMessages)
        {
            const auto &[foreignMessage, senderId, rebroadcastMessage] = newForeignMessage;

            if (isMessageValid(foreignMessage))
            {
                acknowledgment->addToAckList(foreignMessage.data().sequence_number());
                timedMessages += is_test_recording();
            }

            if (foreignMessage.has_ack_count())
            {
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

        for (auto& newForeignMessage : foreignMessages)
        {
            const auto &[foreignMessage, senderId, rebroadcastMessage] = newForeignMessage;
            if (rebroadcastMessage)
            {
                pipeline->rebroadcastToOwnRsm(rebroadcastMessage);
            }
        }
    }
    SPDLOG_INFO("ALL MESSAGES RECEIVED : Receive thread exiting");
    localReceiveThread.join();
    addMetric("foreign_messages_received", timedMessages);
    addMetric("max_acknowledgment", acknowledgment->getAckIterator().value_or(0));
    addMetric("max_quorum_acknowledgment", quorumAck->getCurrentQuack().value_or(0));
}
