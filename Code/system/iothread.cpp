#include "iothread.h"

#include "acknowledgment.h"
#include "crypto.h"
#include "ipc.h"
#include "proto_utils.h"
#include "scrooge_message.pb.h"
#include "scrooge_request.pb.h"
#include "scrooge_transfer.pb.h"

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
void runRelayIPCRequestThread(
    const std::shared_ptr<iothread::MessageQueue<scrooge::CrossChainMessageData>> messageOutput,
    NodeConfiguration kNodeConfiguration)
{
    bindThreadToCpu(1);
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

void runRelayIPCTransactionThread(std::string scroogeOutputPipePath, std::shared_ptr<QuorumAcknowledgment> quorumAck,
                                  NodeConfiguration kNodeConfiguration,
                                  std::shared_ptr<iothread::MessageQueue<scrooge::CrossChainMessage>> receivedMessageQueue)
{
    bindThreadToCpu(1);
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
#if WRITE_DR
    Acknowledgment transferredMessages{};
    scrooge::CrossChainMessage receivedMessage;
    scrooge::ScroogeTransfer drTransfer;
#elif WRITE_CCF
    Acknowledgment transferredMessages{};
    scrooge::CrossChainMessage receivedMessage;
    scrooge::ScroogeTransfer ccfTransfer;
#endif
    while (not is_test_over())
    {
        const auto curQuorumAck = quorumAck->getCurrentQuack();
        if (lastQuorumAck < curQuorumAck)
        {
            lastQuorumAck = curQuorumAck;
            transfer.mutable_commit_acknowledgment()->set_sequence_number(lastQuorumAck.value());
            const auto serializedTransfer = transfer.SerializeAsString();
            // SPDLOG_CRITICAL("Write: {} :: N:{} :: R:{}",lastQuorumAck.value(), kNodeConfiguration.kNodeId,
            // get_rsm_id());
            writeMessage(pipe, serializedTransfer);
        }

#if WRITE_DR
        while (receivedMessageQueue->try_dequeue(receivedMessage))
        {
            for (auto& msg : receivedMessage.data())
            {
                scrooge::KeyValue receivedKeyValue;
                const auto isparseSuccessful = receivedKeyValue.ParseFromString(msg.message_content());
                if (not isparseSuccessful)
                {
                    SPDLOG_CRITICAL("Could not parse DR received KeyValue, received data '{}'", msg.message_content());
                    continue;
                }
                *drTransfer.mutable_key_value_update() = std::move(receivedKeyValue);
                const auto serializedDrTransfer = drTransfer.SerializeAsString();
                writeMessage(pipe, serializedDrTransfer);
            }
        }
#elif WRITE_CCF
        while (receivedMessageQueue->try_dequeue(receivedMessage))
        {
            for (auto& msg : receivedMessage.data())
            {
                scrooge::KeyValue receivedKeyValue;
                const auto isparseSuccessful = receivedKeyValue.ParseFromString(msg.message_content());
                if (not isparseSuccessful)
                {
                    SPDLOG_CRITICAL("Could not parse DR received KeyValue, received data '{}'", msg.message_content());
                    continue;
                }
                *ccfTransfer.mutable_key_value_update() = std::move(receivedKeyValue);
                const auto serializedCcfTransfer = ccfTransfer.SerializeAsString();
                writeMessage(pipe, serializedCcfTransfer);
            }
        }
#endif
    }
    pipe.close();
    addMetric("IPC test", true);
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
