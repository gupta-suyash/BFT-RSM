#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <boost/lockfree/spsc_queue.hpp>

#include "nng/nng.h"
#include <nng/protocol/pipeline0/pull.h>
#include <nng/protocol/pipeline0/push.h>

#include "global.h"

#include "scrooge_message.pb.h"

namespace pipeline
{
struct ReceivedCrossChainMessage
{
    scrooge::CrossChainMessage message;
    uint64_t senderId;
};

struct SendMessageRequest
{
  std::chrono::steady_clock::time_point kRequestCreationTime{};
  uint64_t destinationNodeId{};
  bool isDestinationForeign{};
  scrooge::CrossChainMessage message;
};

using SendMessageRequestQueue = boost::lockfree::spsc_queue<SendMessageRequest>;
}; // namespace pipeline

class Pipeline
{
  public:
    Pipeline(std::vector<std::string>&& ownNetworkUrls, std::vector<std::string>&& otherNetworkUrls, NodeConfiguration ownConfiguration);
    ~Pipeline();

    void startPipeline();

    void SendToOtherRsm(uint64_t receivingNodeId, scrooge::CrossChainMessage&& message);
    std::vector<pipeline::ReceivedCrossChainMessage> RecvFromOtherRsm();

    void BroadcastToOwnRsm(scrooge::CrossChainMessage &&message);
    vector<scrooge::CrossChainMessage> RecvFromOwnRsm();

    

  private:
    void runSendThread(std::unique_ptr<std::vector<nng_socket>> foreignSendSockets, std::unique_ptr<std::vector<nng_socket>> localSendSockets);
    void SetSockets();

    uint64_t getSendPort(uint64_t receiverId, bool isForeign);
    uint64_t getReceivePort(uint64_t senderId, bool isForeign);

    static constexpr uint64_t kMinimumPortNumber = 7000;
    mutable std::mutex mMutex;

    const NodeConfiguration kOwnConfiguration;
    const std::vector<std::string> kOwnNetworkUrls;
    const std::vector<std::string> kOtherNetworkUrls;
    // send sockets owned by sending thread
    // look in Pipeline::runSendThread
    std::vector<nng_socket> mLocalReceiveSockets;
    std::vector<nng_socket> mForeignReceiveSockets;

    std::thread messageSendThread{};
    std::atomic_bool mIsThreadRunning{};
    std::atomic_bool mShouldThreadStop{};
    pipeline::SendMessageRequestQueue mMessageRequests{1024};
};
