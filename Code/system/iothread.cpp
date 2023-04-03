#include "iothread.h"

#include "ipc.h"
#include "scrooge_message.pb.h"
#include "scrooge_request.pb.h"
#include "scrooge_transfer.pb.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <map>
#include <pthread.h>
#include <stdio.h>
#include <thread>

#include <boost/circular_buffer.hpp>

template <typename T>
void blockingPush(moodycamel::BlockingReaderWriterCircularBuffer<T> &queue, T &&message,
                  std::chrono::microseconds pollPeriod)
{
    while (not is_test_over() && not queue.try_enqueue(std::forward<T>(message)))
    {
        std::this_thread::sleep_for(pollPeriod);
    }
}

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

        blockingPush(*messageOutput, std::move(fakeMessage), 10us);
        std::this_thread::sleep_for(10us); // configure for network
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

            scrooge::CrossChainMessage newMessage;
            *newMessage.mutable_data() = newMessageRequest.content();
            *newMessage.mutable_validity_proof() = newMessageRequest.validity_proof();

            blockingPush(*messageOutput, std::move(newMessage), 1us);
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
    bindThreadToCpu(0);
    SPDLOG_INFO("Send Thread starting with TID = {}", gettid());
    constexpr auto kSleepTime = 1ns;

    const MessageScheduler messageScheduler(configuration);

    auto resendMessageMap = std::map<uint64_t, iothread::MessageResendData>{};

    while (not is_test_over())
    {
        scrooge::CrossChainMessage newMessage;
        while (messageInput->try_dequeue(newMessage))
        {
            const auto sequenceNumber = newMessage.data().sequence_number();
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
        for (auto it = resendMessageMap.begin(); it != resendMessageMap.end();)
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
                continue;
            }

            // We got to the message which should be resent
            const auto curNodeFirstResend = firstDestinationResendNumber;
            const auto curNodeLastResend = firstDestinationResendNumber + destinations.size() - 1;
            const auto curNodeCompletedResends = curNodeFirstResend + numDestinationsSent;
            const auto curFinalDestination = std::min<uint64_t>(curNodeLastResend, activeResendData->resendNumber);
            for (uint64_t resend = curNodeCompletedResends; resend <= curFinalDestination; resend++)
            {
                // TODO: Optimize to send all destinations at once with a bitset
                const auto destination = destinations.at(resend - curNodeFirstResend);
                pipeline->SendToOtherRsm(destination, newMessage);
                numDestinationsSent++;
            }

            const auto isComplete = numDestinationsSent == destinations.size();
            if (isComplete)
            {
                it = resendMessageMap.erase(it);
                break;
            }
        }

        std::this_thread::sleep_for(kSleepTime);
    }

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
            writeMessage(pipe, serializedTransfer);
        }
        std::this_thread::sleep_for(100ms);
    }
}

void runReceiveThread(const std::shared_ptr<Pipeline> pipeline, const std::shared_ptr<Acknowledgment> acknowledgment,
                      const std::shared_ptr<AcknowledgmentTracker> ackTracker,
                      const std::shared_ptr<QuorumAcknowledgment> quorumAck, const NodeConfiguration configuration)
{
    bindThreadToCpu(2);
    const auto &[kOwnNetworkSize, kOtherNetworkSize, kOwnNetworkStakes, kOtherNetworkStakes, kOwnMaxNumFailedStake,
                 kOtherMaxNumFailedStake, kNodeId, kLogPath, kWorkingDir] = configuration;

    uint64_t timedMessages{};

    boost::circular_buffer<scrooge::CrossChainMessage> domesticMessages(1024);
    boost::circular_buffer<pipeline::ReceivedCrossChainMessage> foreignMessages(1024);

    while (not is_test_over())
    {
        pipeline->BroadcastToOwnRsm(foreignMessages);

        const auto oldNumForeign = foreignMessages.size();
        domesticMessages.clear();

        pipeline->RecvFromOwnRsm(domesticMessages);
        pipeline->RecvFromOtherRsm(foreignMessages);
        const auto firstNewForeign = std::next(std::cbegin(foreignMessages), oldNumForeign);

        for (auto newDomesticMessage : domesticMessages)
        {
            if (isMessageValid(newDomesticMessage))
            {
                acknowledgment->addToAckList(newDomesticMessage.data().sequence_number());
                timedMessages += is_test_recording();
            }
        }

        for (auto newForeignMessage = firstNewForeign; newForeignMessage != std::cend(foreignMessages);
             newForeignMessage++)
        {
            const auto &[foreignMessage, senderId] = *newForeignMessage;

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
        }
    }
    SPDLOG_INFO("ALL MESSAGES RECEIVED : Receive thread exiting");
    addMetric("foreign_messages_received", timedMessages);
    addMetric("max_acknowledgment", acknowledgment->getAckIterator().value_or(0));
    addMetric("max_quorum_acknowledgment", quorumAck->getCurrentQuack().value_or(0));
}
