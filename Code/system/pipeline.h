#pragma once

#include <atomic>
#include <bitset>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "readerwritercircularbuffer.h"

#include <boost/circular_buffer.hpp>

#include <nng/nng.h>

#include "global.h"

#include "scrooge_message.pb.h"

namespace pipeline
{
struct ReceivedCrossChainMessage
{
    nng_msg *protoBatch{};
    uint64_t senderId{};
};

struct MessageBatch
{
  nng_msg *message{};
  uint64_t spaceUsed{};
  std::chrono::steady_clock::time_point creationTime{};
};

MessageBatch initMessageBatch(const uint64_t batchSize, const uint64_t batchEps, std::chrono::steady_clock::time_point creationTime);


void trimMessageBatch(MessageBatch& batch);

template <typename T> using MessageQueue = moodycamel::BlockingReaderWriterCircularBuffer<T>;
}; // namespace pipeline

class Pipeline
{
  public:
    Pipeline(const std::vector<std::string> &ownNetworkUrls, const std::vector<std::string> &otherNetworkUrls,
             NodeConfiguration ownConfiguration);
    ~Pipeline();

    void startPipeline();

    void SendToOtherRsm(uint64_t receivingNodeId, const scrooge::CrossChainMessage &message);
    void BroadcastToOwnRsm(const scrooge::CrossChainMessage &message);
    bool rebroadcastToOwnRsm(nng_msg *message);

    pipeline::ReceivedCrossChainMessage RecvFromOtherRsm();
    pipeline::ReceivedCrossChainMessage RecvFromOwnRsm();

    void SendToAllOtherRsm(const scrooge::CrossChainMessage &message);

  private:
    void bufferedMessageSend(const scrooge::CrossChainMessage& message,
                             std::optional<pipeline::MessageBatch>* const batch,
                             pipeline::MessageQueue<nng_msg *> * const sendingQueue);
    inline void SendToDestinations(bool isLocal, std::bitset<64> destinations,
                                   const scrooge::CrossChainMessage &message);
    void reportFailedNode(const std::string &nodeUrl, uint64_t nodeId, bool isLocal);
    void runSendThread(std::string sendUrl, pipeline::MessageQueue<nng_msg *> *const sendBuffer,
                       const uint64_t destNodeId, const bool isLocal);
    void runRecvThread(std::string recvUrl, pipeline::MessageQueue<nng_msg *> *const recvBuffer,
                       const uint64_t sendNodeId, const bool isLocal);

    uint64_t getSendPort(uint64_t receiverId, bool isForeign);
    uint64_t getReceivePort(uint64_t senderId, bool isForeign);

    static constexpr uint64_t kMinimumPortNumber = 7'000;
    static constexpr uint64_t kBatchSizeEps = 150; // extra bytes to avoid realloc
    static constexpr uint64_t kMinumBatchSize = (1 << 12) - kBatchSizeEps; // bytes
    static constexpr auto kMaxBatchCreationTime = 10s;
    static constexpr auto kMaxNngBlockingTime = 500ms;
    static constexpr uint64_t kBufferSize = 1024;

    const NodeConfiguration kOwnConfiguration;
    const std::vector<std::string> kOwnNetworkUrls;
    const std::vector<std::string> kOtherNetworkUrls;
    // send/receive sockets owned by sending/receiving thread
    // look in Pipeline::runSendThread and Pipeline::runReceiveThread

    std::atomic_bool mIsPipelineStarted{};
    std::atomic_bool mShouldThreadStop{};

    std::atomic<std::bitset<64>> mAliveNodesLocal{};
    std::atomic<std::bitset<64>> mAliveNodesForeign{};

    std::vector<std::thread> mLocalSendThreads;
    std::vector<std::thread> mLocalRecvThreads;
    std::vector<std::thread> mForeignSendThreads;
    std::vector<std::thread> mForeignRecvThreads;
    std::vector<std::unique_ptr<pipeline::MessageQueue<nng_msg *>>> mLocalSendBufs{};
    std::vector<std::unique_ptr<pipeline::MessageQueue<nng_msg *>>> mLocalRecvBufs{};
    std::vector<std::unique_ptr<pipeline::MessageQueue<nng_msg *>>> mForeignSendBufs{};
    std::vector<std::unique_ptr<pipeline::MessageQueue<nng_msg *>>> mForeignRecvBufs{};

    std::vector<std::optional<pipeline::MessageBatch>> mLocalMessageBatches{};
    std::vector<std::optional<pipeline::MessageBatch>> mForeignMessageBatches{};
};
