#include "iothread.h"

#include "scrooge_message.pb.h"

#include <chrono>
#include <thread>

#include <map>

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
void runGenerateMessageThread(const std::shared_ptr<iothread::MessageQueue> messageOutput, NodeConfiguration configuration)
{
    const auto kNumMessages = get_number_of_packets();
    const auto kMessageSize = get_packet_size();
    const auto kSignatureSize = (configuration.kOwnMaxNumFailedNodes + 1) * 512;

    for (uint64_t curSequenceNumber = 0; true; curSequenceNumber++)
    {
        scrooge::CrossChainMessage fakeMessage;

        scrooge::CrossChainMessageData *const fakeData = fakeMessage.mutable_data();
        fakeData->set_message_content(std::string(kMessageSize, 'L'));
        fakeData->set_sequence_number(curSequenceNumber);

        fakeMessage.set_validity_proof(std::string(kSignatureSize, 'X'));

        blockingPush(*messageOutput, std::move(fakeMessage), 1us);
    }
}

// Relays messages to be sent over ipc
void runRelayIPCMessageThread(const std::shared_ptr<iothread::MessageQueue> messageOutput,
                              const NodeConfiguration configuration)
{
    // not implemented
}

void runSendThread(const std::shared_ptr<iothread::MessageQueue> messageInput, const std::shared_ptr<Pipeline> pipeline,
                   const std::shared_ptr<Acknowledgment> acknowledgment,
                   const std::shared_ptr<AcknowledgmentTracker> ackTracker,
                   const std::shared_ptr<QuorumAcknowledgment> quorumAck, const NodeConfiguration configuration)
{
    constexpr auto kSleepTime = 1us;
    const auto kResendWaitPeriod = 5s;
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
            const bool isReadyForResend = numStaleAcksForSending < numQuackRepeats;
            if (!isReadyForResend)
            {
                break; // exit and re-evaluate later, map is sorted so early exiting is ok
            }

            // resend this message, delete after.
            const auto receiverNode =
                getMessageDestinationId(sequenceNumber, kNodeId, kOwnNetworkSize, kOtherNetworkSize);
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
    std::optional<uint64_t> lastQuackCount;
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
        if (lastAckCount != newAckCount)
        {
            // This is the node's local view of what it has received from the other network
            const auto ackCount = newAckCount.value_or(0);
            const auto ackCountRate = ackCount / timeElapsed;
            SPDLOG_INFO("Node Ack Count now at {} Ack rate = {} /s", ackCount, ackCountRate);
            lastAckCount = newAckCount;
        }

        const auto newQuackCount = quorumAck->getCurrentQuack();
        if (lastQuackCount != newQuackCount)
        {
            // This is the node's idea of what the other cluster's received has from its cluster
            const auto quackCount = newQuackCount.value_or(0);
            const auto quackCountRate = quackCount / timeElapsed;
            SPDLOG_INFO("Node Quack Count now at {} Quack rate: {} /s", quackCount, quackCountRate);
            lastQuackCount = quackCount;
        }
        std::this_thread::sleep_for(kPollTime);
    }
}
