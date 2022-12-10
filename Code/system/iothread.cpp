#include "iothread.h"

#include "scrooge_message.pb.h"

#include <chrono>
#include <vector>

// Returns the node that should be sent a message from a given sender
uint64_t getMessageDestinationId(uint64_t sequenceNumber, uint64_t senderId, uint64_t numNodesInOwnNetwork,
                                 uint64_t numNodesInOtherNetwork)
{
    const auto trueMod = [](int64_t v, int64_t m) -> uint64_t { return ((v % m) + m) % m; };
    const auto msgRound = sequenceNumber / numNodesInOwnNetwork;
    const auto originalSenderId = sequenceNumber % numNodesInOwnNetwork;
    const auto resendNum = trueMod(senderId - originalSenderId, numNodesInOwnNetwork);

    return (msgRound + resendNum + sequenceNumber) % numNodesInOtherNetwork;
}

// Generates fake messages of a given size for throughput testing
void runGenerateMessageThread(const std::shared_ptr<PipeQueue> pipeQueue)
{
    const auto kNumMessages = get_number_of_packets();
    const auto kMessageSize = get_packet_size();
    const auto kSignatureSize = (get_max_nodes_fail(true) + 1) * 256;

    for (uint64_t curSequenceNumber = 0; curSequenceNumber < kNumMessages; curSequenceNumber++)
    {
        scrooge::CrossChainMessage fakeMessage;

        scrooge::CrossChainMessageData *const fakeData = fakeMessage.mutable_data();
        fakeData->set_message_content(std::string(kMessageSize, 'L'));
        fakeData->set_sequence_number(curSequenceNumber);

        fakeMessage.set_validity_proof(std::string(kSignatureSize, 'X'));

        pipeQueue->addMessage(std::move(fakeMessage), std::chrono::steady_clock::now());
    }
}

// Relays messages to be sent over ipc
void runRelayIPCMessageThread(const std::shared_ptr<PipeQueue> pipeQueue)
{
    // not implemented
}

void runSendThread(const std::shared_ptr<PipeQueue> pipeQueue, const std::shared_ptr<Pipeline> pipeline,
                   const std::shared_ptr<Acknowledgment> acknowledgment,
                   const std::shared_ptr<QuorumAcknowledgment> quorumAck)
{
    constexpr auto kMaxSleepTime = 100ms;
    constexpr auto kMinSleepTime = 0ms;
    const auto kCurNodeId = get_node_id();
    const auto kNumNodesInOwnNetwork = get_nodes_rsm();
    const auto kNumNodesInOtherNetwork = get_nodes_other_rsm();
    while (true)
    {
        const auto curTime = std::chrono::steady_clock::now();

        auto readyMessage = pipeQueue->getReadyMessage(curTime);

        const auto receiverQuorum = quorumAck->getCurrentQuack();
        const bool isMessageAlreadyReceived =
            !readyMessage.has_value() || (receiverQuorum >= readyMessage->data().sequence_number());

        if (readyMessage.has_value() && !isMessageAlreadyReceived)
        {
            const auto curAckValue = acknowledgment->getAckIterator();
            const auto sequenceNumber = readyMessage->data().sequence_number();
            const auto receiverNode =
                getMessageDestinationId(sequenceNumber, kCurNodeId, kNumNodesInOwnNetwork, kNumNodesInOtherNetwork);
            if (curAckValue.has_value())
            {
                readyMessage->mutable_ack_count()->set_value(curAckValue.value());
            }

            pipeline->SendToOtherRsm(receiverNode, std::move(readyMessage.value()));
        }

        const auto nextMessageTime = pipeQueue->getNextReadyMessageTime().value_or(curTime + kMaxSleepTime);
        const auto sleepAmount =
            std::clamp<std::chrono::nanoseconds>(nextMessageTime - curTime, kMinSleepTime, kMaxSleepTime);
        std::this_thread::sleep_for(sleepAmount);
    }
}

void runReceiveThread(const std::shared_ptr<Pipeline> pipeline, const std::shared_ptr<Acknowledgment> acknowledgment,
                      const std::shared_ptr<QuorumAcknowledgment> quorumAck)
{
    constexpr auto kPollTime = 1ms;
    std::optional<uint64_t> lastAckCount;
    while (true)
    {
        // kinda sucks, no overlap of sending and receiving data ... could probably make 2x faster
        // in golang this would be like one line :(
        const auto newForeignMessages = pipeline->RecvFromOtherRsm();
        const auto newDomesticMessages = pipeline->RecvFromOwnRsm();

        for (const auto &receivedForeignMessage : newForeignMessages)
        {
            const auto &[foreignMessage, senderId] = receivedForeignMessage;
            pipeline->BroadcastToOwnRsm(foreignMessage);

            acknowledgment->addToAckList(foreignMessage.data().sequence_number());

            if (foreignMessage.has_ack_count())
            {
                const auto foreignAckCount = foreignMessage.ack_count().value();
                quorumAck->updateNodeAck(senderId, foreignAckCount);
            }
        }

        for (const auto &domesticMessage : newDomesticMessages)
        {
            // Only good for updating the ack value -- could add a sleep for 'signature verification'
            acknowledgment->addToAckList(domesticMessage.data().sequence_number());
        }

        const auto newAckCount = acknowledgment->getAckIterator();
        if (lastAckCount != newAckCount)
        {
            SPDLOG_INFO("Node Ack Count Now at {}", newAckCount.value_or(0));
            lastAckCount = newAckCount;
        }
    }
}
