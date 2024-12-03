#pragma once

#include "acknowledgment.h"
#include "config.h"
#include "global.h"

#include "scrooge_message.pb.h"

#include <atomic>
#include <bitset>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "readerwriterqueue.h"
#include <nng/nng.h>

namespace pipeline
{
struct ReceivedCrossChainMessage
{
    nng_msg *message{};
    uint64_t senderId{};
};

struct CrossChainMessageBatch
{
    scrooge::CrossChainMessage data{};
    std::chrono::steady_clock::time_point creationTime{};
    uint64_t batchSizeEstimate{};
};

template <typename T> using MessageQueue = moodycamel::ReaderWriterQueue<T>;
}; // namespace pipeline

class Pipeline
{
  public:
    Pipeline(const std::vector<std::string> &ownNetworkUrls, const std::vector<std::string> &otherNetworkUrls,
             NodeConfiguration ownConfiguration);
    ~Pipeline();

    void startPipeline();

    bool SendToOtherRsm(uint64_t receivingNodeId, scrooge::CrossChainMessageData &&messageData,
                        const Acknowledgment *const acknowledgment, std::chrono::steady_clock::time_point curTime);
    bool SendFileToOtherRsm(uint64_t receivingNodeId, scrooge::CrossChainMessageData &&messageData,
                            const Acknowledgment *const acknowledgment, std::chrono::steady_clock::time_point curTime);
    void forceSendToOtherRsm(uint64_t receivingNodeId, const Acknowledgment *const acknowledgment,
                             std::chrono::steady_clock::time_point curTime);
    void forceSendFileToOtherRsm(uint64_t receivingNodeId, const Acknowledgment *const acknowledgment,
                                 std::chrono::steady_clock::time_point curTime);
    bool rebroadcastToOwnRsm(nng_msg *message);

    pipeline::ReceivedCrossChainMessage RecvFromOtherRsm();
    pipeline::ReceivedCrossChainMessage RecvFromOwnRsm();

    void SendToAllOtherRsm(scrooge::CrossChainMessageData &&message, std::chrono::steady_clock::time_point curTime);
    void SendFileToAllOtherRsm(scrooge::CrossChainMessageData &&message, std::chrono::steady_clock::time_point curTime);
    void SendToGeoBFTQuorumOtherRsm(scrooge::CrossChainMessageData &&message,
                                    std::chrono::steady_clock::time_point curTime);
    void SendFileToGeoBFTQuorumOtherRsm(scrooge::CrossChainMessageData &&message,
                                        std::chrono::steady_clock::time_point curTime);

  private:
    bool bufferedMessageSend(scrooge::CrossChainMessageData &&message, pipeline::CrossChainMessageBatch *const batch,
                             const Acknowledgment *const acknowledgment,
                             pipeline::MessageQueue<nng_msg *> *const sendingQueue,
                             std::chrono::steady_clock::time_point curTime);
    bool bufferedFileMessageSend(scrooge::CrossChainMessageData &&message,
                                 pipeline::CrossChainMessageBatch *const batch,
                                 const Acknowledgment *const acknowledgment,
                                 pipeline::MessageQueue<nng_msg *> *const sendingQueue,
                                 std::chrono::steady_clock::time_point curTime);
    void flushBufferedMessage(pipeline::CrossChainMessageBatch *const batch, const Acknowledgment *const acknowledgment,
                              pipeline::MessageQueue<nng_msg *> *const sendingQueue,
                              std::chrono::steady_clock::time_point curTime);
    void flushBufferedFileMessage(pipeline::CrossChainMessageBatch *const batch,
                                  const Acknowledgment *const acknowledgment,
                                  pipeline::MessageQueue<nng_msg *> *const sendingQueue,
                                  std::chrono::steady_clock::time_point curTime);
    void reportFailedNode(const std::string &nodeUrl, uint64_t nodeId, bool isLocal);
    void runSendThread(std::string sendUrl, pipeline::MessageQueue<nng_msg *> *const sendBuffer,
                       const uint64_t destNodeId, const bool isLocal);
    void runRecvThread(std::string recvUrl, pipeline::MessageQueue<nng_msg *> *const recvBuffer,
                       const uint64_t sendNodeId, const bool isLocal);

    uint64_t getSendPort(uint64_t receiverId, bool isForeign);
    uint64_t getReceivePort(uint64_t senderId, bool isForeign);

    static constexpr uint64_t kMinimumPortNumber = 7'000;
    static constexpr uint64_t kProtobufDefaultSize = kListSize / 8;
    static constexpr uint64_t kMinimumBatchSize = BATCH_SIZE; // bytes
    // batches can be larger than kMinimumBatchSize, but the total excess will be <= kMaxBudgetDeficit
    static constexpr uint64_t kMaxBudgetDeficit = 26214 * 8 * 4; // bytes
    static constexpr auto kMaxBatchCreationTime = 1ms;
    static constexpr auto kMaxNngBlockingTime = MAX_NNG_BLOCKING_TIME;
    static constexpr uint64_t kBufferSize = PIPELINE_BUFFER_SIZE;

    const NodeConfiguration kOwnConfiguration;
    const std::vector<std::string> kOwnNetworkUrls;
    const std::vector<std::string> kOtherNetworkUrls;
    // send/receive sockets owned by sending/receiving thread
    // look in Pipeline::runSendThread and Pipeline::runReceiveThread

    int64_t mCurBudgetDeficit{};

    std::atomic_bool mIsPipelineStarted{};
    std::atomic_bool mShouldThreadStop{};

    std::bitset<64> mAliveNodesLocal{};
    std::bitset<64> mAliveNodesForeign{};

    std::vector<std::thread> mLocalSendThreads;
    std::vector<std::thread> mLocalRecvThreads;
    std::vector<std::thread> mForeignSendThreads;
    std::vector<std::thread> mForeignRecvThreads;
    std::vector<std::unique_ptr<pipeline::MessageQueue<nng_msg *>>> mLocalSendBufs{};
    std::vector<std::unique_ptr<pipeline::MessageQueue<nng_msg *>>> mLocalRecvBufs{};
    std::vector<std::unique_ptr<pipeline::MessageQueue<nng_msg *>>> mForeignSendBufs{};
    std::vector<std::unique_ptr<pipeline::MessageQueue<nng_msg *>>> mForeignRecvBufs{};

    std::vector<pipeline::CrossChainMessageBatch> mForeignMessageBatches{};
};
