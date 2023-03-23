#pragma once

#include <atomic>
#include <bitset>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "readerwritercircularbuffer.h"

#include <boost/circular_buffer.hpp>

#include "nng/nng.h"
#include <nng/protocol/pipeline0/pull.h>
#include <nng/protocol/pipeline0/push.h>

#include "global.h"

#include "scrooge_message.pb.h"

namespace pipeline
{
struct ReceivedCrossChainMessage
{
    scrooge::CrossChainMessage message{};
    uint64_t senderId{};
};

struct nng_message
{
    size_t dataSize{};
    char *data{}; // free with nng_free()
};

struct foreign_nng_message
{
    nng_message message{};
    uint64_t senderId{};
};

struct SendMessageRequest
{
    using DestinationSet = std::bitset<64>;
    std::chrono::steady_clock::time_point kRequestCreationTime{};
    DestinationSet destinationNodes{};
    std::string messageData;
};

template <typename T> using MessageQueue = moodycamel::BlockingReaderWriterCircularBuffer<T>;
}; // namespace pipeline

class Pipeline
{
  public:
    Pipeline(std::vector<std::string> &&ownNetworkUrls, std::vector<std::string> &&otherNetworkUrls,
             NodeConfiguration ownConfiguration);
    ~Pipeline();

    void startPipeline();

    void SendToOtherRsm(uint64_t receivingNodeId, scrooge::CrossChainMessage &message);

    void BroadcastToOwnRsm(boost::circular_buffer<pipeline::ReceivedCrossChainMessage> &in);

    void RecvFromOtherRsm(boost::circular_buffer<pipeline::ReceivedCrossChainMessage> &out);
    void RecvFromOwnRsm(boost::circular_buffer<scrooge::CrossChainMessage> &out);

    void SendToAllOtherRsm(scrooge::CrossChainMessage &message);

	void BroadcastKeyToOwnRsm();
	void BroadcastKeyToOtherRsm();
	void RecvFromOwnRsm();
	void RecvFromOtherRsm();

  private:
    void runForeignSendThread(std::unique_ptr<std::vector<nng_socket>> foreignSendSockets);
    void runLocalSendThread(std::unique_ptr<std::vector<nng_socket>> localSendSockets);
    void runForeignReceiveThread(std::unique_ptr<std::vector<nng_socket>> foreignReceiveSockets);
    void runLocalReceiveThread(std::unique_ptr<std::vector<nng_socket>> localReceiveSockets);

    uint64_t getSendPort(uint64_t receiverId, bool isForeign);
    uint64_t getReceivePort(uint64_t senderId, bool isForeign);

    static constexpr uint64_t kMinimumPortNumber = 7000;
    static constexpr uint64_t kBatchSize = 750000;

    const NodeConfiguration kOwnConfiguration;
    const std::vector<std::string> kOwnNetworkUrls;
    const std::vector<std::string> kOtherNetworkUrls;
    // send/receive sockets owned by sending/receiving thread
    // look in Pipeline::runSendThread and Pipeline::runReceiveThread

    std::thread mForeignMessageSendThread{};
    std::thread mForeignMessageReceiveThread{};
    std::thread mLocalMessageSendThread{};
    std::thread mLocalMessageReceiveThread{};
    std::atomic_bool mIsPipelineStarted{};
    std::atomic_bool mShouldThreadStop{};
    pipeline::MessageQueue<pipeline::SendMessageRequest> mMessageRequestsLocal{4096};
    pipeline::MessageQueue<pipeline::SendMessageRequest> mMessageRequestsForeign{4096};
    pipeline::MessageQueue<pipeline::foreign_nng_message> mForeignReceivedMessageQueue{4096};
    pipeline::MessageQueue<pipeline::nng_message> mLocalReceivedMessageQueue{4096};
};
