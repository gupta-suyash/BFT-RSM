#pragma once

#include "global.h"
#include "acknowledgment.h"
#include "readerwritercircularbuffer.h"

#include "scrooge_message.pb.h"

#include <atomic>
#include <bitset>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <boost/circular_buffer.hpp>
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
  uint64_t batchSizeEstimate{};
};

template <typename T> using MessageQueue = moodycamel::BlockingReaderWriterCircularBuffer<T>;
}; // namespace pipeline

class Pipeline
{
  public:
    Pipeline(const std::vector<std::string> &ownNetworkUrls, const std::vector<std::string> &otherNetworkUrls,
             NodeConfiguration ownConfiguration);
    ~Pipeline();

    void startPipeline();

    bool SendToOtherRsm(uint64_t receivingNodeId, scrooge::CrossChainMessageData &&messageData, const Acknowledgment * const acknowledgment);
    bool rebroadcastToOwnRsm(nng_msg *message);

    pipeline::ReceivedCrossChainMessage RecvFromOtherRsm();
    pipeline::ReceivedCrossChainMessage RecvFromOwnRsm();

    void SendToAllOtherRsm(scrooge::CrossChainMessageData &&message);

  private:
    bool bufferedMessageSend(scrooge::CrossChainMessageData &&message,
                             pipeline::CrossChainMessageBatch *const batch,
                             const Acknowledgment * const acknowledgment,
                             pipeline::MessageQueue<nng_msg *> *const sendingQueue);
    void flushBufferedMessage(pipeline::CrossChainMessageBatch *const batch,
                              const Acknowledgment* const acknowledgment,
                              pipeline::MessageQueue<nng_msg *> *const sendingQueue);
    void reportFailedNode(const std::string &nodeUrl, uint64_t nodeId, bool isLocal);
    void runSendThread(std::string sendUrl, pipeline::MessageQueue<nng_msg *> *const sendBuffer,
                       const uint64_t destNodeId, const bool isLocal);
    void runRecvThread(std::string recvUrl, pipeline::MessageQueue<nng_msg *> *const recvBuffer,
                       const uint64_t sendNodeId, const bool isLocal);

    uint64_t getSendPort(uint64_t receiverId, bool isForeign);
    uint64_t getReceivePort(uint64_t senderId, bool isForeign);

    static constexpr uint64_t kMinimumPortNumber = 7'000;
    static constexpr uint64_t kProtobufDefaultSize = 0;
    static constexpr uint64_t kMinimumBatchSize = (1 << 18); // bytes
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

    std::vector<pipeline::CrossChainMessageBatch> mForeignMessageBatches{};
};
