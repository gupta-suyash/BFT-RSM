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

// Relays messages to be sent over ipc
void runRelayIPCRequestThread(
    const std::shared_ptr<iothread::MessageQueue<scrooge::CrossChainMessageData>> messageOutput,
    NodeConfiguration kNodeConfiguration)
{
    SPDLOG_CRITICAL("#############Inside run relay IPC REQUEST THREAD!");
    bindThreadToCpu(1);
    constexpr auto kScroogeInputPath = "/tmp/scrooge-input";
    Acknowledgment receivedMessages{};
    uint64_t numReceivedMessages{};

    uint64_t startingMetric{};
    auto lastMetric = std::chrono::steady_clock::now();
    const auto startingTime = std::chrono::steady_clock::now();

    //createPipe(kScroogeInputPath);
    std::ifstream pipe{kScroogeInputPath};
    if (!pipe.is_open())
    {
        SPDLOG_CRITICAL("########################Reader Open Failed={}, {}", std::strerror(errno), getlogin());
    }
    else
    {
        SPDLOG_CRITICAL("###########################Reader Open Success");
    }

    while (not is_test_over())
    {
        //SPDLOG_CRITICAL("BEFORE READING");
        auto messageBytes = readMessage(pipe);
        if (messageBytes.length() == 0) {
            SPDLOG_CRITICAL("PIPE READ ERROR");
            break;
        }   
        //SPDLOG_CRITICAL("RIGHT AFTER READING");
        scrooge::ScroogeRequest newRequest;
        //SPDLOG_CRITICAL("ABOUT TO PARSE THE REQUEST");
        const auto isParseSuccessful = newRequest.ParseFromString(std::move(messageBytes));
        if (not isParseSuccessful)
        {
            SPDLOG_CRITICAL("FAILED TO READ MESSAGE");
            continue;
        }

        numReceivedMessages++;
        const auto curTime = std::chrono::steady_clock::now();
        if (curTime - startingTime >= 5s)
        {
            if (startingMetric == 0)
                startingMetric = numReceivedMessages;
            if (curTime - lastMetric >= 1s)
            {
                SPDLOG_CRITICAL("CUR THROUGHPUT: {}", (numReceivedMessages - startingMetric) / std::chrono::duration<double>(curTime - startingTime - 5s).count());
                lastMetric = curTime;
            }
        }
        switch (newRequest.request_case())
        {
            using request = scrooge::ScroogeRequest::RequestCase;
        case request::kSendMessageRequest: {
            auto newMessageRequest = newRequest.send_message_request();
            receivedMessages.addToAckList(newMessageRequest.content().sequence_number());
            while (not messageOutput->try_enqueue(std::move(*(newMessageRequest.mutable_content()))) &&
                not is_test_over());
            break;
            //SPDLOG_CRITICAL("GOING TO ADD MESSAGE WITH SEQ NO {}", newMessageRequest.content().sequence_number());
        }
        default: {
            SPDLOG_ERROR("UNKNOWN REQUEST TYPE {}", newRequest.request_case());
            //SPDLOG_CRITICAL("DO NOT KNOW THE REQUEST TYPE");
        }
        }
    }
    SPDLOG_CRITICAL("END OF WHILE LOOP RELAY IPC");

    addMetric("ipc_recv_messages", numReceivedMessages);
    addMetric("ipc_msg_block_size", receivedMessages.getAckIterator().value_or(0));
    SPDLOG_INFO("Relay IPC Message Thread Exiting");
}

void runRelayIPCTransactionThread(std::string scroogeOutputPipePath, std::shared_ptr<QuorumAcknowledgment> quorumAck,
                                  NodeConfiguration kNodeConfiguration,
                                  std::shared_ptr<iothread::MessageQueue<scrooge::CrossChainMessage>> receivedMessageQueue)
{
    SPDLOG_CRITICAL("###############Inside runRelayIPCTransactionThread which write scrooge-output!");
    bindThreadToCpu(1);
    std::ofstream pipe{scroogeOutputPipePath, std::ios_base::app};
    if (!pipe.is_open())
    {
        SPDLOG_CRITICAL("######################Write Open Failed={}, {}", std::strerror(errno), getlogin());
    }
    else
    {
        SPDLOG_CRITICAL("########################Writer Open Success");
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
        std::this_thread::sleep_for(1ms);
        const auto curQuorumAck = quorumAck->getCurrentQuack();
        if (lastQuorumAck < curQuorumAck)
        {
            while (lastQuorumAck < curQuorumAck)
            {
                //SPDLOG_CRITICAL("QUACK ACTUALLY SENT! CurQuack: {}", curQuorumAck.value());
                lastQuorumAck = lastQuorumAck.value_or(-1ULL) + 1;
                transfer.mutable_commit_acknowledgment()->set_sequence_number(lastQuorumAck.value());
                const auto serializedTransfer = transfer.SerializeAsString();
                writeMessage(pipe, serializedTransfer);
            }
            lastQuorumAck = curQuorumAck;
            transfer.mutable_commit_acknowledgment()->set_sequence_number(lastQuorumAck.value());
            const auto serializedTransfer = transfer.SerializeAsString();
            // SPDLOG_CRITICAL("Write: {} :: N:{} :: R:{}",lastQuorumAck.value(), kNodeConfiguration.kNodeId,
            // get_rsm_id());
            writeMessage(pipe, serializedTransfer);
        }

#if WRITE_DR
        if (get_rsm_id() == 0)
        {
            continue;
        }
        while (receivedMessageQueue->try_dequeue(receivedMessage))
        {
            *drTransfer.mutable_unvalidated_cross_chain_message() = std::move(receivedMessage);
            const auto serializedDrTransfer = drTransfer.SerializeAsString();
            writeMessage(pipe, serializedDrTransfer);
        }
#elif WRITE_CCF
        while (receivedMessageQueue->try_dequeue(receivedMessage))
        {
            for (auto& msg : receivedMessage.data())
            {
                scrooge::KeyValueHash receivedKeyValue;
                const auto isparseSuccessful = receivedKeyValue.ParseFromString(msg.message_content());

                // SPDLOG_CRITICAL("Key bytes {} value_hash bytes {}", stringToHex(receivedKeyValue.key()), receivedKeyValue.value_md5_hash());
                if (not isparseSuccessful)
                {
                    SPDLOG_CRITICAL("Could not parse DR received KeyValueHash, received data '{}'", msg.message_content());
                    continue;
                }
                *ccfTransfer.mutable_key_value_hash() = std::move(receivedKeyValue);
                const auto serializedCcfTransfer = ccfTransfer.SerializeAsString();
                writeMessage(pipe, serializedCcfTransfer);
            }
        }
#endif
    }
    SPDLOG_CRITICAL("END OF WHILE LOOP TRANSACTION IPC.");
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
