#include "iothread.h"

#include "ipc.h"
#include "scrooge_message.pb.h"
#include "scrooge_request.pb.h"

#include <chrono>
#include <map>
#include <thread>

uint64_t trueMod(int64_t value, int64_t modulus)
{
    const auto remainder = (value % modulus);

    if (remainder < 0)
    {
        return remainder + modulus;
    }

    return remainder;
}

template <typename T>
void blockingPush(iothread::MessageQueue &queue, T &&message, std::chrono::duration<double> pollPeriod)
{
    while (not queue.push(std::forward<T>(message)))
    {
        std::this_thread::sleep_for(pollPeriod);
    }
}

// Returns the node that should be sent a message from a given sender
uint64_t getMessageDestinationId(uint64_t sequenceNumber, uint64_t senderId, uint64_t numNodesInOwnNetwork,
                                 uint64_t numNodesInOtherNetwork)
{
    const auto msgRound = sequenceNumber / numNodesInOwnNetwork;
    const auto originalSenderId = sequenceNumber % numNodesInOwnNetwork;
    const auto resendNum = trueMod(senderId - originalSenderId, numNodesInOwnNetwork);

    return (originalSenderId + msgRound + resendNum) % numNodesInOtherNetwork;
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
    const auto kNumMessages = get_number_of_packets();
    const auto kMessageSize = get_packet_size();
    const auto kSignatureSize = (configuration.kOwnMaxNumFailedNodes + 1) * 512;

    for (uint64_t curSequenceNumber = 0; curSequenceNumber < kNumMessages; curSequenceNumber++)
    {
        scrooge::CrossChainMessage fakeMessage;

        scrooge::CrossChainMessageData *const fakeData = fakeMessage.mutable_data();
        fakeData->set_message_content(std::string(kMessageSize, 'L'));
        fakeData->set_sequence_number(curSequenceNumber);

        fakeMessage.set_validity_proof(std::string(kSignatureSize, 'X'));

        blockingPush(*messageOutput, std::move(fakeMessage), 100us);
        std::this_thread::sleep_for(300us); // configure for network
    }
}

// Relays messages to be sent over ipc
void runRelayIPCRequestThread(const std::shared_ptr<iothread::MessageQueue> messageOutput)
{
    const auto kNumMessages = get_number_of_packets();
    constexpr auto kScroogeInputPath = "/tmp/scrooge-input";
    const auto readMessages = std::make_shared<ipc::DataChannel>(10);
    const auto exitReader = std::make_shared<std::atomic_bool>();

    createPipe(kScroogeInputPath);
    auto reader = std::thread(startPipeReader, kScroogeInputPath, readMessages, exitReader);

    while (true)
    {
        constexpr auto kPollPeriod = 10us;

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

            blockingPush(*messageOutput, std::move(newMessage), kPollPeriod);
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

void runSendThread(const std::shared_ptr<iothread::MessageQueue> messageInput, const std::shared_ptr<Pipeline> pipeline,
                   const std::shared_ptr<Acknowledgment> acknowledgment,
                   const std::shared_ptr<AcknowledgmentTracker> ackTracker,
                   const std::shared_ptr<QuorumAcknowledgment> quorumAck, const NodeConfiguration configuration)
{
    constexpr auto kSleepTime = 1us;
    const auto &[kOwnNetworkSize, kOtherNetworkSize, kOwnMaxNumFailedNodes, kOtherMaxNumFailedNodes, kNodeId] =
        configuration;
    const auto kMaxMessageSends = kOwnMaxNumFailedNodes + kOtherMaxNumFailedNodes + 1;

    // auto sendMessageBuffer = std::vector<scrooge::CrossChainMessage>{};
    auto resendMessageMap = std::map<uint64_t, scrooge::CrossChainMessage>{};

    while (true)
    {
        // Send and store new messages
        // TODO Benchmark if it is better to empty the queue sending optimistically or retry first
        // TODO Implement multithreaded sending to parallelize sending messages (or does this matter w sockets?)
        scrooge::CrossChainMessage newMessage;
        while (messageInput->pop(newMessage))
        {
            const auto sequenceNumber = newMessage.data().sequence_number();
            const auto originalSenderId = sequenceNumber % kOwnNetworkSize;

            if (originalSenderId == kNodeId)
            {
                // sendMessageBuffer.emplace_back(std::move(newMessage));
                const auto receiverNode =
                    getMessageDestinationId(sequenceNumber, kNodeId, kOwnNetworkSize, kOtherNetworkSize);

                setAckValue(&newMessage, *acknowledgment);

                pipeline->SendToOtherRsm(receiverNode, std::move(newMessage));
                continue;
            }

            const auto numPreviousSenders = trueMod(kNodeId - originalSenderId, kOwnNetworkSize);
            const bool shouldThisNodeAlsoSend = (numPreviousSenders + 1) <= kMaxMessageSends;
            if (shouldThisNodeAlsoSend)
            {
                resendMessageMap.emplace(sequenceNumber, std::move(newMessage));
            }
        }

        const auto curQuack = quorumAck->getCurrentQuack();
        if (!curQuack.has_value())
        {
            continue; // Wait for messages before resending
            // TODO Bug, if message 0 is not sent, nobody will resend
        }

        const auto numQuackRepeats = ackTracker->getAggregateRepeatedAckCount(curQuack.value());
        for (auto it = std::begin(resendMessageMap); it != std::end(resendMessageMap); it = resendMessageMap.erase(it))
        {
            auto &[sequenceNumber, message] = *it;

            const bool isMessageAlreadyReceived = sequenceNumber < curQuack.value();
            if (isMessageAlreadyReceived)
            {
                continue; // delete this message
            }

            const auto originalSenderId = sequenceNumber % kOwnNetworkSize;
            const auto numPreviousSenders = trueMod(kNodeId - originalSenderId, kOwnNetworkSize);
            const auto numStaleAcksForSending = (kOtherMaxNumFailedNodes + 1) * numPreviousSenders;
            const bool isReadyForResend = numStaleAcksForSending <= numQuackRepeats;
            if (!isReadyForResend)
            {
                break; // exit and re-evaluate later, map is sorted so early exiting is ok
            }

            // resend this message, delete after.
            const auto receiverNode =
                getMessageDestinationId(sequenceNumber, kNodeId, kOwnNetworkSize, kOtherNetworkSize);

            setAckValue(&message, *acknowledgment);

            pipeline->SendToOtherRsm(receiverNode, std::move(message));
        }

        std::this_thread::sleep_for(kSleepTime);
    }
}

void runReceiveThread(const std::shared_ptr<Pipeline> pipeline, const std::shared_ptr<Acknowledgment> acknowledgment,
                      const std::shared_ptr<AcknowledgmentTracker> ackTracker,
                      const std::shared_ptr<QuorumAcknowledgment> quorumAck, const NodeConfiguration configuration)
{
    constexpr auto kPollTime = 1us;
    const auto kStartTime = std::chrono::steady_clock::now();

    std::optional<uint64_t> lastAckCount;
    while (true)
    {
        const auto curTime = std::chrono::steady_clock::now();
        const double timeElapsed = std::chrono::duration<double>(curTime - kStartTime).count();
        const auto newDomesticMessages = pipeline->RecvFromOwnRsm();
        auto newForeignMessages = pipeline->RecvFromOtherRsm();

        for (const auto &domesticMessage : newDomesticMessages)
        {
            if (isMessageValid(domesticMessage))
            {
                acknowledgment->addToAckList(domesticMessage.data().sequence_number());
            }
        }

        for (auto &receivedForeignMessage : newForeignMessages)
        {
            auto &[foreignMessage, senderId] = receivedForeignMessage;

            if (isMessageValid(foreignMessage))
            {
                acknowledgment->addToAckList(foreignMessage.data().sequence_number());
            }

            if (foreignMessage.has_ack_count())
            {
                const auto foreignAckCount = foreignMessage.ack_count().value();
                quorumAck->updateNodeAck(senderId, foreignAckCount);
                ackTracker->updateNodeData(senderId, foreignAckCount, curTime);
            }

            pipeline->BroadcastToOwnRsm(std::move(foreignMessage));
        }

        const auto newAckCount = acknowledgment->getAckIterator();
        const auto newQuackCount = quorumAck->getCurrentQuack();
        if (lastAckCount != newAckCount)
        {
            // This is the node's local view of what it has received from the other network
            const auto ackCount = newAckCount.value_or(-1);
            const auto ackCountRate = ackCount / timeElapsed;
            const auto quackCount = newQuackCount.value_or(-1);
            const auto quackCountRate = quackCount / timeElapsed;
            SPDLOG_INFO("Node Ack Count now at {} Ack rate = {} /s Quack count {} rate = {}", ackCount, ackCountRate,
                        quackCount, quackCountRate);
            lastAckCount = newAckCount;
        }
        std::this_thread::sleep_for(kPollTime);
    }
}
