#include "iothread.h"

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
/*    const auto startTime = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - startTime < 30s)
    {
        readMessage(pipe);
        
    }
*/
    while (not is_test_over())
    {
        //SPDLOG_CRITICAL("BEFORE READING");
        auto messageBytes = readMessage(pipe);
        //SPDLOG_CRITICAL("RIGHT AFTER READING");
        scrooge::ScroogeRequest newRequest;
        //SPDLOG_CRITICAL("ABOUT TO PARSE THE REQUEST");
        const auto isParseSuccessful = newRequest.ParseFromString(std::move(messageBytes));
        if (not isParseSuccessful)
        {
            SPDLOG_CRITICAL("FAILED TO READ MESSAGE");
            continue;
        }
        //SPDLOG_CRITICAL("WE'VE GOT MAIL");
        switch (newRequest.request_case())
        {
            using request = scrooge::ScroogeRequest::RequestCase;
        case request::kSendMessageRequest: {
            auto newMessageRequest = newRequest.send_message_request();
            receivedMessages.addToAckList(newMessageRequest.content().sequence_number());
            SPDLOG_CRITICAL("GOING TO ADD MESSAGE WITH SEQ NO {}", newMessageRequest.content().sequence_number());
            while (not messageOutput->try_enqueue(std::move(*(newMessageRequest.mutable_content()))) &&
                   not is_test_over())
                std::this_thread::sleep_for(10us);
            break;
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
                                  NodeConfiguration kNodeConfiguration)
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
    const auto mutableCommitAck = transfer.mutable_commit_acknowledgment();
    while (not is_test_over())
    {
        const auto curQuorumAck = quorumAck->getCurrentQuack();
        if (lastQuorumAck < curQuorumAck)
        {
            SPDLOG_CRITICAL("QUACK ACTUALLY SENT! CurQuack: {}", curQuorumAck.value());
            lastQuorumAck = curQuorumAck;
            mutableCommitAck->set_sequence_number(lastQuorumAck.value());
            const auto serializedTransfer = transfer.SerializeAsString();
            writeMessage(pipe, serializedTransfer);
            SPDLOG_CRITICAL("Successfully wrote quack {}",lastQuorumAck.value());
        }
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
