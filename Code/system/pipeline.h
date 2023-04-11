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
    scrooge::CrossChainMessage message{};
    uint64_t senderId{};
    nng_msg *rebroadcastMsg{};
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

    void SendToOtherRsm(uint64_t receivingNodeId, const scrooge::CrossChainMessage &message);
    void BroadcastToOwnRsm(const scrooge::CrossChainMessage &message);
    void rebroadcastToOwnRsm(nng_msg *message);

    void RecvFromOtherRsm(boost::circular_buffer<pipeline::ReceivedCrossChainMessage> &out);
    void RecvFromOwnRsm(boost::circular_buffer<scrooge::CrossChainMessage> &out);
    void RecvAllToAllFromOtherRsm(boost::circular_buffer<scrooge::CrossChainMessage> &out);

    void SendToAllOtherRsm(const scrooge::CrossChainMessage &message);

    void BroadcastKeyToOwnRsm();
    void BroadcastKeyToOtherRsm();
    void RecvFromOwnRsm();
    void RecvFromOtherRsm();

  private:
    inline void SendToDestinations(bool isLocal, std::bitset<64> destinations,
                                   const scrooge::CrossChainMessage &message);
    void reportFailedNode(const std::string &nodeUrl, uint64_t nodeId, bool isLocal);
    void runSendThread(std::string sendUrl, pipeline::MessageQueue<nng_msg *> *const sendBuffer,
                       const uint64_t destNodeId, const bool isLocal);
    void runRecvThread(std::string recvUrl, pipeline::MessageQueue<nng_msg *> *const recvBuffer,
                       const uint64_t sendNodeId, const bool isLocal);

    uint64_t getSendPort(uint64_t receiverId, bool isForeign);
    uint64_t getReceivePort(uint64_t senderId, bool isForeign);

    static constexpr uint64_t kMinimumPortNumber = 7000;
    static constexpr std::chrono::milliseconds kMaxNngBlockingTime{500ms};
    static constexpr uint64_t kBufferSize = 256;

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
};
