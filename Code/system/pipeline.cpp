#include "pipeline.h"

#include "acknowledgment.h"
#include "crypto.h"

#include <algorithm>
#include <boost/container/small_vector.hpp>
#include <boost/circular_buffer.hpp>
#include <nng/protocol/pair1/pair.h>
#include <sched.h>

int64_t numTimeoutHits{}, numSizeHits{}, totalBatchedMessages{}, totalBatchesSent{};

static int64_t getLogAck(const scrooge::CrossChainMessage &message)
{
    if (!message.has_ack_count())
    {
        return -1;
    }
    return message.ack_count().value();
}

static void generateMessageMac(scrooge::CrossChainMessage *const message)
{
    // MAC signing TODO
    const auto mstr = message->ack_count().SerializeAsString();
    // Sign with own key.
    std::string encoded = CmacSignString(get_priv_key(), mstr);
    message->set_validity_proof(encoded);
    // std::cout << "Mac: " << encoded << std::endl;
}

static void setAckValue(scrooge::CrossChainMessage *const message, const Acknowledgment &acknowledgment)
{
    const auto curAckView = acknowledgment.getAckView<(kListSize)>();
    const auto ackIterator = acknowledgment::getAckIterator(curAckView);
    if (ackIterator.has_value())
    {
        message->mutable_ack_count()->set_value(ackIterator.value());
    }
    *message->mutable_ack_set() = {curAckView.view.begin(),
                                   std::find(curAckView.view.begin(), curAckView.view.end(), 0)};
}

template <typename atomic_bitset> void reset_atomic_bit(atomic_bitset &set, uint64_t bit)
{
    auto curSet = set.load();
    while (true)
    {
        auto newSet = curSet;
        newSet.reset(bit);
        if (set.compare_exchange_weak(curSet, newSet))
        {
            return;
        }
    }
}

void openSendRecvSocket(const std::string &sendUrl, const std::string &recvUrl, nng_socket* socket, std::chrono::milliseconds maxNngBlockingTime)
{
    constexpr auto kSuccess = 0;

    const auto openResult = nng_pair1_open(socket);
    const bool cannotOpenSocket = kSuccess != openResult;
    if (cannotOpenSocket)
    {
        SPDLOG_CRITICAL("Cannot open socket for send/recv on URL send {} recv {} ERROR: {}", sendUrl, recvUrl, nng_strerror(openResult));
        std::abort();
    }

    const auto nngListenResult = nng_listen(*socket, recvUrl.c_str(), nullptr, 0);

    if (nngListenResult != 0)
    {
        SPDLOG_CRITICAL("CANNOT OPEN SOCKET FOR LISTENING on URL = '{}' Return value {}", recvUrl,
                        nng_strerror(nngListenResult));
        std::abort();
    }

    const auto nngDialResult = nng_dial(*socket, sendUrl.c_str(), nullptr, NNG_FLAG_NONBLOCK);

    if (nngDialResult != kSuccess)
    {
        SPDLOG_CRITICAL("CANNOT OPEN SOCKET FOR SENDING on URL = '{}' Return value {}", sendUrl, nng_strerror(nngDialResult));
        std::abort();
    }


    const long kDesiredMemoryUsage = 12ULL * (1ULL << 30);
    const auto kNumSocketsTotal = 2 * (OWN_RSM_SIZE + OTHER_RSM_SIZE);
    const long kNumOfBufferedElements =
        std::min<long>(64, (double)kDesiredMemoryUsage / kNumSocketsTotal / std::max<long>(250000, PACKET_SIZE));
    addMetric("socket-buffer-size-receive", kNumOfBufferedElements);
    bool nngSetRecvTimeoutResult = nng_socket_set_ms(*socket, NNG_OPT_RECVTIMEO, maxNngBlockingTime.count());
    if (nngSetRecvTimeoutResult != 0)
    {
        SPDLOG_CRITICAL("Cannot set timeout for listening url {} Return value {}", sendUrl,
                        nng_strerror(nngSetRecvTimeoutResult));
        std::abort();
    }
    bool nngSetSendTimeoutResult = nng_setopt_ms(*socket, NNG_OPT_SENDTIMEO, maxNngBlockingTime.count());
    if (nngSetSendTimeoutResult != kSuccess)
    {
        SPDLOG_CRITICAL("Cannot set timeout for sending Return value {}", sendUrl, nng_strerror(nngSetSendTimeoutResult));
        std::abort();
    }
    bool nngSetSndBufSizeResult = nng_socket_set_int(*socket, NNG_OPT_SENDBUF, kNumOfBufferedElements);
    if (nngSetSndBufSizeResult != 0)
    {
        SPDLOG_CRITICAL("Cannot set send buf size {} for url {} RV {}", kNumOfBufferedElements, sendUrl,
                        nng_strerror(nngSetSndBufSizeResult));
        std::abort();
    }
    bool nngSetRecBufSizeResult = nng_socket_set_int(*socket, NNG_OPT_RECVBUF, kNumOfBufferedElements);
    if (nngSetRecBufSizeResult != 0)
    {
        SPDLOG_CRITICAL("Cannot set rec buf size {} for url {} RV {}", kNumOfBufferedElements, sendUrl,
                        nng_strerror(nngSetRecBufSizeResult));
        std::abort();
    }
    bool nngSetRcvMaxSize = nng_socket_set_size(*socket, NNG_OPT_RECVMAXSZ, 0);
    if (nngSetRcvMaxSize != 0)
    {
        SPDLOG_CRITICAL("Cannot set max receive size for url {} RV {}", sendUrl, nng_strerror(nngSetRcvMaxSize));
        std::abort();
    }
}

static nng_socket openReceiveSocket(const std::string &url, std::chrono::milliseconds maxNngBlockingTime)
{
    constexpr auto kResultOpenSuccessful = 0;

    nng_socket socket;
    const auto openResult = nng_pair1_open(&socket);
    const bool cannotOpenSocket = kResultOpenSuccessful != openResult;
    if (cannotOpenSocket)
    {
        SPDLOG_CRITICAL("Cannot open socket for receiving on URL {} ERROR: {}", url, nng_strerror(openResult));
        std::abort();
    }

    const auto nngListenResult = nng_listen(socket, url.c_str(), nullptr, 0);

    if (nngListenResult != 0)
    {
        SPDLOG_CRITICAL("CANNOT OPEN SOCKET FOR LISTENING. URL = '{}' Return value {}", url,
                        nng_strerror(nngListenResult));
        std::abort();
    }

    const long kDesiredMemoryUsage = 12ULL * (1ULL << 30);
    const auto kNumSocketsTotal = 2 * (OWN_RSM_SIZE + OTHER_RSM_SIZE);
    const long kNumOfBufferedElements =
        std::min<long>(64, (double)kDesiredMemoryUsage / kNumSocketsTotal / std::max<long>(250000, PACKET_SIZE));
    addMetric("socket-buffer-size-receive", kNumOfBufferedElements);
    bool nngSetTimeoutResult = nng_socket_set_ms(socket, NNG_OPT_RECVTIMEO, maxNngBlockingTime.count());
    if (nngSetTimeoutResult != 0)
    {
        SPDLOG_CRITICAL("Cannot set timeout for listening url {} Return value {}", url,
                        nng_strerror(nngSetTimeoutResult));
        std::abort();
    }
    bool nngSetSndBufSizeResult = nng_socket_set_int(socket, NNG_OPT_SENDBUF, kNumOfBufferedElements);
    if (nngSetSndBufSizeResult != 0)
    {
        SPDLOG_CRITICAL("Cannot set send buf size {} for url {} RV {}", kNumOfBufferedElements, url,
                        nng_strerror(nngSetSndBufSizeResult));
        std::abort();
    }
    bool nngSetRecBufSizeResult = nng_socket_set_int(socket, NNG_OPT_RECVBUF, kNumOfBufferedElements);
    if (nngSetRecBufSizeResult != 0)
    {
        SPDLOG_CRITICAL("Cannot set rec buf size {} for url {} RV {}", kNumOfBufferedElements, url,
                        nng_strerror(nngSetRecBufSizeResult));
        std::abort();
    }
    bool nngSetRcvMaxSize = nng_socket_set_size(socket, NNG_OPT_RECVMAXSZ, 0);
    if (nngSetRcvMaxSize != 0)
    {
        SPDLOG_CRITICAL("Cannot set max receive size for url {} RV {}", url, nng_strerror(nngSetRcvMaxSize));
        std::abort();
    }

    return socket;
}

static nng_socket openSendSocket(const std::string &url, std::chrono::milliseconds maxNngBlockingTime)
{
    constexpr auto kSuccess = 0;

    nng_socket socket;
    const auto openResult = nng_pair1_open(&socket);

    const bool cannotOpenSocket = kSuccess != openResult;
    if (cannotOpenSocket)
    {
        SPDLOG_CRITICAL("Cannot open socket for sending on URL {} ERROR: {}", url, nng_strerror(openResult));
        std::abort();
    }

    const auto nngDialResult = nng_dial(socket, url.c_str(), nullptr, NNG_FLAG_NONBLOCK);

    if (nngDialResult != kSuccess)
    {
        SPDLOG_CRITICAL("CANNOT OPEN SOCKET FOR SENDING. URL = '{}' Return value {}", url, nng_strerror(nngDialResult));
        std::abort();
    }

    bool nngSetTimeoutResult = nng_setopt_ms(socket, NNG_OPT_SENDTIMEO, maxNngBlockingTime.count());
    if (nngSetTimeoutResult != kSuccess)
    {
        SPDLOG_CRITICAL("Cannot set timeout for sending Return value {}", url, nng_strerror(nngSetTimeoutResult));
        std::abort();
    }

    const long kDesiredMemoryUsage = 12ULL * (1ULL << 30);
    const auto kNumSocketsTotal = 2 * (OWN_RSM_SIZE + OTHER_RSM_SIZE);
    const long kNumOfBufferedElements =
        std::min<long>(64, (double)kDesiredMemoryUsage / kNumSocketsTotal / std::max<long>(250000, PACKET_SIZE));
    addMetric("socket-buffer-size-send", kNumOfBufferedElements);
    bool nngSetSndBufSizeResult = nng_socket_set_int(socket, NNG_OPT_SENDBUF, kNumOfBufferedElements);
    if (nngSetSndBufSizeResult != 0)
    {
        SPDLOG_CRITICAL("Cannot set send buf size {} for url {} RV {}", kNumOfBufferedElements, url,
                        nng_strerror(nngSetSndBufSizeResult));
        std::abort();
    }
    bool nngSetRecBufSizeResult = nng_socket_set_int(socket, NNG_OPT_RECVBUF, kNumOfBufferedElements);
    if (nngSetRecBufSizeResult != 0)
    {
        SPDLOG_CRITICAL("Cannot set rec buf size {} for url {} RV {}", kNumOfBufferedElements, url,
                        nng_strerror(nngSetRecBufSizeResult));
        std::abort();
    }
    bool nngSetRcvMaxSize = nng_socket_set_size(socket, NNG_OPT_RECVMAXSZ, 0);
    if (nngSetRcvMaxSize != 0)
    {
        SPDLOG_CRITICAL("Cannot set max receive size for url {} RV {}", url, nng_strerror(nngSetRcvMaxSize));
        std::abort();
    }

    return socket;
}



/* Sending data to nodes of other RSM.
 *
 * @param buf is the outgoing message of protobuf type.
 * @param socket is the nng_socket to put the data into
 */
static int sendMessage(const nng_socket &socket, nng_msg** message)
{
    const auto sendReturnValue = nng_sendmsg(socket, *message, NNG_FLAG_NONBLOCK);
    const bool isActualError = sendReturnValue != 0 && sendReturnValue != nng_errno_enum::NNG_EAGAIN &&
                               sendReturnValue != nng_errno_enum::NNG_ETIMEDOUT;
    if (isActualError)
    {
        SPDLOG_CRITICAL("nng_send has error value = {}", nng_strerror(sendReturnValue));
    }
    if (sendReturnValue)
    {
        *message = nullptr;
    }

    return sendReturnValue;
}

/* Receving data from nodes of other RSM.
 *
 * @param socket is the nng_socket to check for data on.
 * @return the protobuf if data for one was contained in the socket.
 */
static std::optional<nng_msg *> receiveMessage(const nng_socket &socket)
{
    nng_msg *msg;

    const auto receiveReturnValue = nng_recvmsg(socket, &msg, NNG_FLAG_NONBLOCK);

    if (receiveReturnValue != 0)
    {
        if (receiveReturnValue != nng_errno_enum::NNG_EAGAIN && receiveReturnValue != nng_errno_enum::NNG_ETIMEDOUT)
        {
            SPDLOG_CRITICAL("nng_recv has error value = {}", nng_strerror(receiveReturnValue));
        }

        return std::nullopt;
    }

    return msg;
}

static void closeSocket(nng_socket &socket, std::chrono::steady_clock::time_point closeTime)
{
    // NNG says to wait before closing
    // https://nng.nanomsg.org/man/tip/nng_close.3.html
    // const auto bufferTime = 2s;
    // std::this_thread::sleep_until(closeTime + bufferTime);
    // nng_close(socket); <- This hangs the entire program.... Thanks nng....
}

static nng_msg *serializeProtobuf(scrooge::CrossChainMessage &proto)
{
    const auto protoSize = proto.ByteSizeLong();
    nng_msg *message;
    const auto allocResult = nng_msg_alloc(&message, protoSize) == 0;
    char *const messageData = (char *)nng_msg_body(message);

    bool success = allocResult && proto.SerializeToArray(messageData, protoSize);
    // SPDLOG_CRITICAL("Static data is now set: {} and the ByteSizeLong() is {}", proto.data().size(), protoSize);
    if (not success)
    {
        SPDLOG_CRITICAL("PROTO SERIALIZE FAILED DataSize={} AllocResult={}", protoSize, allocResult);
        std::abort();
    }

    return message;
}

static nng_msg *serializeFileProtobuf(scrooge::CrossChainMessage &proto)
{
    thread_local std::string staticData = std::string(get_packet_size(), 'L');
    for (auto &crossChainData : *(proto.mutable_data()))
    {
        // SPDLOG_CRITICAL("Added data again");
        crossChainData.set_allocated_message_content(&staticData);
    }
    // SPDLOG_CRITICAL("Static data is now set: {}", proto.data().size());
    const auto protoSize = proto.ByteSizeLong();
    // SPDLOG_CRITICAL("Static data is now set: {} and the ByteSizeLong() is {}", proto.data().size(), protoSize);
    nng_msg *message;
    const auto allocResult = nng_msg_alloc(&message, protoSize) == 0;
    char *const messageData = (char *)nng_msg_body(message);

    bool success = allocResult && proto.SerializeToArray(messageData, protoSize);

    for (auto &crossChainData : *(proto.mutable_data()))
    {
        crossChainData.release_message_content();
    }

    if (not success)
    {
        // SPDLOG_CRITICAL("PROTO SERIALIZE FAILED DataSize={} AllocResult={}", protoSize, allocResult);
        std::abort();
    }

    return message;
}

Pipeline::Pipeline(const std::vector<std::string> &ownNetworkUrls, const std::vector<std::string> &otherNetworkUrls,
                   NodeConfiguration ownConfiguration)
    : kOwnConfiguration(ownConfiguration), kOwnNetworkUrls(ownNetworkUrls), kOtherNetworkUrls(otherNetworkUrls),
      mForeignMessageBatches(kOwnConfiguration.kOtherNetworkSize)
{
    std::bitset<64> foreignAliveNodes{}, localAliveNodes{};
    localAliveNodes |= -1ULL ^ (-1ULL << kOwnConfiguration.kOwnNetworkSize);
    foreignAliveNodes |= -1ULL ^ (-1ULL << kOwnConfiguration.kOtherNetworkSize);
    mAliveNodesLocal = localAliveNodes;
    mAliveNodesForeign = foreignAliveNodes;

    addMetric("Batch Size", kMinimumBatchSize);
    addMetric("Batch Timeout", std::chrono::duration<double>(kMaxBatchCreationTime).count());
    addMetric("Pipeline Buffer Size", kBufferSize);
}

Pipeline::~Pipeline()
{
    addMetric("num-timeout-hits", numTimeoutHits);
    addMetric("num-size-hits", numSizeHits);
    addMetric("avg-num-msgs-in-batch", (double)totalBatchedMessages / totalBatchesSent);
    const auto joinThread = [](std::thread &thread) { thread.join(); };

    mShouldThreadStop = true;

    std::for_each(mWorkerThreads.begin(), mWorkerThreads.end(), joinThread);
}

/* Returns the port the current node will use to receive from senderId
 * Current port strategy is that all nodes will listen to local traffic from node i on port `kMinimumPortNumber + i`
 * All nodes will also listen to foreign traffic from node j on port `kMinimumPortNumber + kSizeOfOwnNetwork + j`
 */
uint64_t Pipeline::getReceivePort(uint64_t senderId, bool isForeign)
{
    if (isForeign)
    {
        return kMinimumPortNumber + kOwnConfiguration.kOwnNetworkSize + senderId;
    }
    return kMinimumPortNumber + senderId;
}

/* Returns the port the current node will use to send to receiverId
 * Current port strategy is that Node i will listen to traffic from local node i on port `kMinimumPortNumber + i`
 * Node i will listen to traffic from a forign node j on port `kMinimumPortNumber + kSizeOfOwnNetwork + j`
 */
uint64_t Pipeline::getSendPort(uint64_t receiverId, bool isForeign)
{
    if (isForeign)
    {
        return kMinimumPortNumber + kOwnConfiguration.kOtherNetworkSize + kOwnConfiguration.kNodeId;
    }
    return kMinimumPortNumber + kOwnConfiguration.kNodeId;
}

void Pipeline::startPipeline()
{
    const auto isStarted = mIsPipelineStarted.exchange(true);
    if (isStarted)
    {
        return;
    }
    const auto &kOwnUrl = kOwnNetworkUrls.at(kOwnConfiguration.kNodeId);

    SPDLOG_INFO("Configuring local pipeline threads");
    for (size_t localNodeId = 0; localNodeId < kOwnNetworkUrls.size(); localNodeId++)
    {
        if (kOwnConfiguration.kNodeId == localNodeId)
        {
            mLocalSendBufs.emplace_back();
            mLocalRecvBufs.emplace_back();
            mLocalSendUrls.push_back("");
            mLocalRecvUrls.push_back("");
            continue;
        }

        constexpr bool kIsLocal = true;
        const auto &localUrl = kOwnNetworkUrls.at(localNodeId);
        const auto sendingPort = getSendPort(localNodeId, false);
        const auto receivingPort = getReceivePort(localNodeId, false);

        auto sendingUrl = "tcp://" + localUrl + ":" + std::to_string(sendingPort);
        auto receivingUrl = "tcp://" + kOwnUrl + ":" + std::to_string(receivingPort);

        mLocalRecvBufs.push_back(std::make_unique<pipeline::MessageQueue<scrooge::CrossChainMessage>>(kBufferSize));
        mLocalSendUrls.push_back(sendingUrl);
        mLocalRecvUrls.push_back(receivingUrl);
    }

    SPDLOG_INFO("Configuring foreign pipeline threads");
    for (size_t foreignNodeId = 0; foreignNodeId < kOtherNetworkUrls.size(); foreignNodeId++)
    {
        constexpr auto kIsLocal = false;
        const auto &foreignUrl = kOtherNetworkUrls.at(foreignNodeId);
        const auto sendingPort = getSendPort(foreignNodeId, true);
        const auto receivingPort = getReceivePort(foreignNodeId, true);

        auto sendingUrl = "tcp://" + foreignUrl + ":" + std::to_string(sendingPort);
        auto receivingUrl = "tcp://" + kOwnUrl + ":" + std::to_string(receivingPort);

        mForeignSendBufs.push_back(std::make_unique<pipeline::MessageQueue<scrooge::CrossChainMessage>>(kBufferSize));
        mForeignRecvBufs.push_back(
            std::make_unique<pipeline::MessageQueue<std::shared_ptr<scrooge::CrossChainMessage>>>(kBufferSize));
        mForeignSendUrls.push_back(sendingUrl);
        mForeignRecvUrls.push_back(receivingUrl);
    }

    for (uint64_t i{}; i < kNumWorkerThreads; i++)
    {
        mLocalSendBufs.push_back(
            std::make_unique<pipeline::MessageQueue<std::shared_ptr<scrooge::CrossChainMessage>>>(kBufferSize));
    }

    for (uint64_t workerId{}; workerId < kNumWorkerThreads; workerId++)
    {
        mWorkerThreads.push_back(std::thread(&Pipeline::runWorkerThread, this, workerId));
    }
}

void Pipeline::runWorkerThread(uint64_t workerId)
{
    // workerId is 0..(kNumWorkerThreads-1)
    const uint64_t localNodesPerWorker = std::ceil(kOwnConfiguration.kOwnNetworkSize / kNumWorkerThreads);
    const uint64_t foreignNodesPerWorker = std::ceil(kOwnConfiguration.kOtherNetworkSize / kNumWorkerThreads);
    uint64_t pFirstLocalNode = localNodesPerWorker * workerId;
    uint64_t pLastLocalNode = std::min<uint64_t>(kOwnConfiguration.kOwnNetworkSize, localNodesPerWorker * (workerId + 1)) - 1;
    const uint64_t firstLocalNode = (pFirstLocalNode == kOwnConfiguration.kNodeId)? pFirstLocalNode + 1 : pFirstLocalNode;
    const uint64_t lastLocalNode = (pLastLocalNode == kOwnConfiguration.kNodeId)? std::min<uint64_t>(0, pLastLocalNode - 1) : pLastLocalNode;
    const uint64_t firstForeignNode = foreignNodesPerWorker * workerId;
    const uint64_t lastForeignNode = std::min<uint64_t>(kOwnConfiguration.kOtherNetworkSize, foreignNodesPerWorker * (workerId + 1)) - 1;
    
    std::vector<nng_socket> localSockets{kOwnConfiguration.kOwnNetworkSize};
    std::vector<nng_socket> foreignSockets{kOwnConfiguration.kOtherNetworkSize};
    std::vector<boost::circular_buffer<nng_msg*>> localMsgRebroadcasts(kOwnConfiguration.kOwnNetworkSize, boost::circular_buffer<nng_msg*>(1024));
    std::vector<nng_msg*> pendingForeignSends(kOwnConfiguration.kOtherNetworkSize);
    std::vector<boost::circular_buffer<scrooge::CrossChainMessage>> receivedLocalMessages(kOwnConfiguration.kOwnNetworkSize, boost::circular_buffer<scrooge::CrossChainMessage>(1024));
    std::vector<boost::circular_buffer<std::shared_ptr<scrooge::CrossChainMessage>>> receivedForeignMessages(kOwnConfiguration.kOtherNetworkSize, boost::circular_buffer<std::shared_ptr<scrooge::CrossChainMessage>>(1024));
    scrooge::CrossChainMessage msg;

    for (uint64_t curForeignNode = firstForeignNode; curForeignNode <= lastForeignNode; curForeignNode++)
    {
        openSendRecvSocket(mForeignSendUrls[curForeignNode], mForeignRecvUrls[curForeignNode], &(foreignSockets[curForeignNode]), 0ms);
    }

    for (uint64_t curLocalNode = firstLocalNode; curLocalNode <= lastLocalNode; curLocalNode++)
    {
        if (curLocalNode == kOwnConfiguration.kNodeId)
        {
            continue;
        }
        openSendRecvSocket(mLocalSendUrls[curLocalNode], mLocalRecvUrls[curLocalNode], &(localSockets[curLocalNode]), 0ms);
    }


    while (not mShouldThreadStop.load(std::memory_order_relaxed))
    {
        // service all foreign node sends
        for (uint64_t curForeignNode = firstForeignNode; curForeignNode <= lastForeignNode; curForeignNode++)
        {
            if (pendingForeignSends[curForeignNode])
            {
                sendMessage(foreignSockets[curForeignNode], &(pendingForeignSends[curForeignNode]));
            }

            if (not mForeignSendBufs[curForeignNode]->try_dequeue(msg))
            {
                continue;
            }

            if constexpr (FILE_RSM)
            {
                pendingForeignSends[curForeignNode] = serializeFileProtobuf(msg);
            }
            else
            {
                pendingForeignSends[curForeignNode] = serializeProtobuf(msg);
            }
            sendMessage(foreignSockets[curForeignNode], &(pendingForeignSends[curForeignNode]));
        }

        // service all foreign node recvs
        for (uint64_t curForeignNode = firstForeignNode; curForeignNode <= lastForeignNode; curForeignNode++)
        {
            while (not receivedForeignMessages[curForeignNode].empty() && mForeignRecvBufs[curForeignNode]->try_enqueue(std::move(receivedForeignMessages[curForeignNode].front())))
            {
                receivedForeignMessages[curForeignNode].pop_front();
            }

            std::optional<nng_msg *> curMessage{};
            while (not receivedForeignMessages[curForeignNode].full() && (curMessage = receiveMessage(foreignSockets[curForeignNode])).has_value())
            {
                auto parsedMessage = std::make_shared<scrooge::CrossChainMessage>();
                const auto messageData = nng_msg_body(*curMessage);
                const auto messageSize = nng_msg_len(*curMessage);
                bool success = parsedMessage->ParseFromArray(messageData, messageSize);
                nng_msg_free(curMessage.value());
                if (not (receivedForeignMessages[curForeignNode].empty() && mForeignRecvBufs[curForeignNode]->try_enqueue(std::move(parsedMessage))))
                {
                    // message wasn't able to be directly placed in recv queue. Store it for later
                    receivedForeignMessages[curForeignNode].push_back(std::move(parsedMessage));
                }
            }
        }
        
        if constexpr (ALL_TO_ALL || ONE_TO_ONE)
        {
            continue;
        }
        // Check if we service local nodes
        if (firstLocalNode > lastLocalNode)
        {
            continue;
        }

        // service all local node sends
        uint64_t mostFullBuffer = localMsgRebroadcasts[firstLocalNode].size();
        for (uint64_t curLocalNode = firstLocalNode + 1; curLocalNode <= lastLocalNode; curLocalNode++)
        {
            mostFullBuffer = std::max(mostFullBuffer, localMsgRebroadcasts[curLocalNode].size());
        }

        uint64_t availableMessageSpaces = localMsgRebroadcasts.front().max_size() - mostFullBuffer;
        std::shared_ptr<scrooge::CrossChainMessage> curMsg;
        while (availableMessageSpaces && mLocalSendBufs[workerId]->try_dequeue(curMsg))
        {
            availableMessageSpaces -= 1;
            if constexpr (FILE_RSM)
            {
                localMsgRebroadcasts[firstLocalNode].push_back(serializeFileProtobuf(msg));
            }
            else
            {
                localMsgRebroadcasts[firstLocalNode].push_back(serializeProtobuf(msg));
            }
            
            for (uint64_t curLocalNode = firstLocalNode + 1; curLocalNode <= lastLocalNode; curLocalNode++)
            {
                if (curLocalNode == kOwnConfiguration.kNodeId)
                {
                    continue;
                }
                localMsgRebroadcasts[curLocalNode].push_back(nullptr);
                nng_msg_dup(&localMsgRebroadcasts[curLocalNode].back(), localMsgRebroadcasts[firstLocalNode].back());
            }
        }

        for (uint64_t curLocalNode = firstLocalNode + 1; curLocalNode <= lastLocalNode; curLocalNode++)
        {
            while (not localMsgRebroadcasts[curLocalNode].empty())
            {
                if (sendMessage(localSockets[curLocalNode], &localMsgRebroadcasts[curLocalNode].front()))
                {
                    localMsgRebroadcasts[curLocalNode].pop_front();
                }
            }
        }

        // service all local node recvs
        for (uint64_t curLocalNode = firstLocalNode; curLocalNode <= lastLocalNode; curLocalNode++)
        {
            if (curLocalNode == kOwnConfiguration.kNodeId)
            {
                continue;
            }
            while (not receivedLocalMessages[curLocalNode].empty() && mLocalRecvBufs[curLocalNode]->try_enqueue(std::move(receivedLocalMessages[curLocalNode].front())))
            {
                receivedLocalMessages[curLocalNode].pop_front();
            }
            while (not receivedLocalMessages[curLocalNode].full())
            {
                auto message = receiveMessage(localSockets[curLocalNode]);
                if (not message)
                {
                    break;
                }
                scrooge::CrossChainMessage parsedMessage;
                const auto messageData = nng_msg_body(*message);
                const auto messageSize = nng_msg_len(*message);
                bool success = parsedMessage.ParseFromArray(messageData, messageSize);
                nng_msg_free(message.value());
                if (not (receivedLocalMessages[curLocalNode].empty() && mLocalRecvBufs[curLocalNode]->try_enqueue(std::move(parsedMessage))))
                {
                    receivedLocalMessages[curLocalNode].push_back(std::move(parsedMessage));
                }
            }
        }
    }
}


void Pipeline::flushBufferedMessage(pipeline::CrossChainMessageBatch *const batch,
                                    const Acknowledgment *const acknowledgment,
                                    pipeline::MessageQueue<scrooge::CrossChainMessage> *const sendingQueue,
                                    std::chrono::steady_clock::time_point curTime)
{
    const bool isBatchOversized = batch->batchSizeEstimate > kMinimumBatchSize;
    if (isBatchOversized)
    {
        mCurBudgetDeficit -= batch->batchSizeEstimate - kMinimumBatchSize;
    }

    if (acknowledgment)
    {
        setAckValue(&batch->data, *acknowledgment);
        generateMessageMac(&batch->data);
    }

    totalBatchesSent++;
    totalBatchedMessages += batch->data.data_size();

    batch->batchSizeEstimate = 0; // kProtobufDefaultSize;

    bool pushFailure{};

    while ((pushFailure = not sendingQueue->try_enqueue(std::move(batch->data))) && not is_test_over())
        ;

    batch->creationTime = curTime;
}

void Pipeline::flushBufferedFileMessage(pipeline::CrossChainMessageBatch *const batch,
                                        const Acknowledgment *const acknowledgment,
                                        pipeline::MessageQueue<scrooge::CrossChainMessage> *const sendingQueue,
                                        std::chrono::steady_clock::time_point curTime)
{
    const bool isBatchOversized = batch->batchSizeEstimate > kMinimumBatchSize;
    if (isBatchOversized)
    {
        mCurBudgetDeficit -= batch->batchSizeEstimate - kMinimumBatchSize;
    }

    if (acknowledgment)
    {
        setAckValue(&batch->data, *acknowledgment);
        generateMessageMac(&batch->data);
    }

    totalBatchesSent++;
    totalBatchedMessages += batch->data.data_size();

    batch->batchSizeEstimate = kProtobufDefaultSize;

    bool pushFailure{};

    while ((pushFailure = not sendingQueue->try_enqueue(std::move(batch->data))) && not is_test_over())
        ;

    batch->creationTime = curTime;
}

bool Pipeline::bufferedMessageSend(scrooge::CrossChainMessageData &&message,
                                   pipeline::CrossChainMessageBatch *const batch,
                                   const Acknowledgment *const acknowledgment,
                                   pipeline::MessageQueue<scrooge::CrossChainMessage> *const sendingQueue,
                                   std::chrono::steady_clock::time_point curTime)
{
    const auto newMessageSize = message.ByteSizeLong();
    const auto newBatchSize = batch->batchSizeEstimate + newMessageSize;
    if (batch->batchSizeEstimate > kMinimumBatchSize)
    {
        mCurBudgetDeficit += newMessageSize;
    }
    else if (newBatchSize > kMinimumBatchSize)
    {
        mCurBudgetDeficit += newMessageSize - (kMinimumBatchSize - batch->batchSizeEstimate);
    }
    batch->batchSizeEstimate += newMessageSize;
    batch->data.mutable_data()->Add(std::move(message));

    const auto timeDelta = curTime - batch->creationTime;
    const bool isBatchLargeEnough = batch->batchSizeEstimate >= kMinimumBatchSize;
    const bool isBatchOldEnough = timeDelta >= kMaxBatchCreationTime;

    const bool shouldSend = isBatchLargeEnough || isBatchOldEnough;
    numTimeoutHits += timeDelta >= kMaxBatchCreationTime;
    numSizeHits += batch->batchSizeEstimate >= kMinimumBatchSize;
    if (not shouldSend)
    {
        return false;
    }

    const bool isQueueReady = sendingQueue->size_approx() != 0;
    const bool isBudgetSpent = mCurBudgetDeficit > kMaxBudgetDeficit;
    if (isBudgetSpent or isQueueReady)
    {
        flushBufferedMessage(batch, acknowledgment, sendingQueue, curTime);
        return true;
    }

    return false;
}
bool Pipeline::bufferedFileMessageSend(scrooge::CrossChainMessageData &&message,
                                       pipeline::CrossChainMessageBatch *const batch,
                                       const Acknowledgment *const acknowledgment,
                                       pipeline::MessageQueue<scrooge::CrossChainMessage> *const sendingQueue,
                                       std::chrono::steady_clock::time_point curTime)
{
    const auto newMessageSize = message.ByteSizeLong() + get_packet_size();
    const auto newBatchSize = batch->batchSizeEstimate + newMessageSize;
    if (batch->batchSizeEstimate > kMinimumBatchSize)
    {
        mCurBudgetDeficit += newMessageSize;
    }
    else if (newBatchSize > kMinimumBatchSize)
    {
        mCurBudgetDeficit += newMessageSize - (kMinimumBatchSize - batch->batchSizeEstimate);
    }
    batch->batchSizeEstimate = newBatchSize;
    batch->data.mutable_data()->Add(std::move(message));

    const auto timeDelta = curTime - batch->creationTime;
    const bool isBatchLargeEnough = batch->batchSizeEstimate >= kMinimumBatchSize;
    const bool isBatchOldEnough = timeDelta >= kMaxBatchCreationTime;

    const bool shouldSend = isBatchLargeEnough || isBatchOldEnough;
    numTimeoutHits += timeDelta >= kMaxBatchCreationTime;
    numSizeHits += batch->batchSizeEstimate >= kMinimumBatchSize;
    if (not shouldSend)
    {
        return false;
    }

    const bool isQueueReady = sendingQueue->size_approx() != 0;
    const bool isBudgetSpent = mCurBudgetDeficit > kMaxBudgetDeficit;
    if (isBudgetSpent or isQueueReady)
    {
        flushBufferedFileMessage(batch, acknowledgment, sendingQueue, curTime);
        return true;
    }

    return false;
}
void Pipeline::forceSendToOtherRsm(uint64_t receivingNodeId, const Acknowledgment *const acknowledgment,
                                   std::chrono::steady_clock::time_point curTime)
{
    const auto &destinationBuffer = mForeignSendBufs.at(receivingNodeId);
    const auto destinationBatch = mForeignMessageBatches.data() + receivingNodeId;
    flushBufferedMessage(destinationBatch, acknowledgment, destinationBuffer.get(), curTime);
}
void Pipeline::forceSendFileToOtherRsm(uint64_t receivingNodeId, const Acknowledgment *const acknowledgment,
                                       std::chrono::steady_clock::time_point curTime)
{
    const auto &destinationBuffer = mForeignSendBufs.at(receivingNodeId);
    const auto destinationBatch = mForeignMessageBatches.data() + receivingNodeId;
    flushBufferedFileMessage(destinationBatch, acknowledgment, destinationBuffer.get(), curTime);
}
/* This function is used to send message to a specific node in other RSM.
 *
 * @param nid is the identifier of the node in the other RSM.
 */
bool Pipeline::SendToOtherRsm(uint64_t receivingNodeId, scrooge::CrossChainMessageData &&messageData,
                              const Acknowledgment *const acknowledgment, std::chrono::steady_clock::time_point curTime)
{
    SPDLOG_DEBUG("Queueing Send message to other RSM: nodeId = {}, message = [SequenceId={}, AckId={}, size='{}']",
                 receivingNodeId, message.data().sequence_number(), getLogAck(message),
                 message.data().message_content().size());

    const auto &destinationBuffer = mForeignSendBufs.at(receivingNodeId);
    const auto destinationBatch = mForeignMessageBatches.data() + receivingNodeId;

    return bufferedMessageSend(std::move(messageData), destinationBatch, acknowledgment, destinationBuffer.get(),
                               curTime);
}

/* This function is used to send message to a specific node in other RSM.
 *
 * @param nid is the identifier of the node in the other RSM.
 */
bool Pipeline::SendFileToOtherRsm(uint64_t receivingNodeId, scrooge::CrossChainMessageData &&messageData,
                                  const Acknowledgment *const acknowledgment,
                                  std::chrono::steady_clock::time_point curTime)
{
    SPDLOG_DEBUG("Queueing Send message to other RSM: nodeId = {}, message = [SequenceId={}, AckId={}, size='{}']",
                 receivingNodeId, message.data().sequence_number(), getLogAck(message),
                 message.data().message_content().size());

    const auto &destinationBuffer = mForeignSendBufs.at(receivingNodeId);
    const auto destinationBatch = mForeignMessageBatches.data() + receivingNodeId;

    return bufferedFileMessageSend(std::move(messageData), destinationBatch, acknowledgment, destinationBuffer.get(),
                                   curTime);
}

/* This function is used to send message to a specific node in other RSM.
 *
 * @param nid is the identifier of the node in the other RSM.
 */
void Pipeline::SendToAllOtherRsm(scrooge::CrossChainMessageData &&messageData,
                                 std::chrono::steady_clock::time_point curTime)
{
    SendFileToAllOtherRsm(std::move(messageData), curTime); // I think this works?
}
/* This function is used to send message to a specific node in other RSM.
 *
 * @param nid is the identifier of the node in the other RSM.
 */
void Pipeline::SendFileToAllOtherRsm(scrooge::CrossChainMessageData &&messageData,
                                     std::chrono::steady_clock::time_point curTime)
{
    auto batchCreationTime = &mForeignMessageBatches.front().creationTime;
    auto batch = &mForeignMessageBatches.front().data;
    auto batchSize = &mForeignMessageBatches.front().batchSizeEstimate;

    const auto newDataSize = messageData.ByteSizeLong() + get_packet_size();
    batch->mutable_data()->Add(std::move(messageData));
    *batchSize += newDataSize;

    bool shouldSend = *batchSize >= kMinimumBatchSize || kMaxBatchCreationTime < curTime - *batchCreationTime;
    if (not shouldSend)
    {
        return;
    }
    totalBatchesSent++;
    totalBatchedMessages += batch->data_size();
    numTimeoutHits += kMaxBatchCreationTime < curTime - *batchCreationTime;
    numSizeHits += *batchSize >= kMinimumBatchSize;

    auto remainingForeignNodes = mAliveNodesForeign;

    while (remainingForeignNodes.any() && not is_test_over())
    {
        const auto curDestination = std::countr_zero(remainingForeignNodes.to_ulong());
        remainingForeignNodes.reset(curDestination);
        const auto &curBuffer = mForeignSendBufs.at(curDestination);
        const bool isLastIteration = remainingForeignNodes.none();

        if (isLastIteration)
        {
            while (not curBuffer->try_enqueue(std::move(*batch)))
            {
                if (is_test_over())
                {
                    break;
                }
            }
        }
        else
        {
            while (not curBuffer->try_enqueue(*batch))
            {
                if (is_test_over())
                {
                    break;
                }
            }
        }
    }
    *batchSize = 0;
    *batchCreationTime = curTime;
}

bool Pipeline::rebroadcastToOwnRsm(std::shared_ptr<scrooge::CrossChainMessage> &message)
{
    static std::bitset<64> remainingDestinations{};

    const bool isContinuation = remainingDestinations.any();

    if (not isContinuation)
    {
        remainingDestinations |= -1ULL ^ (-1ULL << kNumWorkerThreads);
        remainingDestinations.reset(kOwnConfiguration.kNodeId);
    }

    std::bitset<64> failedSends{};
    while (remainingDestinations.any() && not is_test_over())
    {
        const auto curDestination = std::countr_zero(remainingDestinations.to_ulong());
        remainingDestinations.reset(curDestination);
        const auto &curBuffer = mLocalSendBufs.at(curDestination);

        const auto curSN = (message->data_size() > 0) ? message->data().at(0).sequence_number() : 0;
        // SPDLOG_CRITICAL("ATTEMPTING REBROADCAST {} msgs TO {} first msg {}", message->data_size(), curDestination,
        // curSN);
        bool isFinalSend = remainingDestinations.none() && failedSends.none();
        if (isFinalSend)
        {
            failedSends[curDestination] = !curBuffer->try_enqueue(std::move(message));
        }
        else
        {
            failedSends[curDestination] = !curBuffer->try_enqueue(message);
        }
    }
    remainingDestinations = failedSends;

    return remainingDestinations.none();
}

/* This function is used to receive messages from the other RSM.
 *
 */
pipeline::ForeignCrossChainMessage Pipeline::RecvFromOtherRsm()
{
    static uint64_t curNode{0ULL - 1};

    pipeline::ForeignCrossChainMessage receivedMessage;

    curNode = (curNode + 1 == mForeignRecvBufs.size()) ? 0 : curNode + 1;

    for (uint64_t node = curNode; node < mForeignRecvBufs.size(); node++)
    {
        const auto &curBuf = mForeignRecvBufs.at(node);
        if (curBuf->try_dequeue(receivedMessage.message))
        {
            receivedMessage.senderId = node;
            return receivedMessage;
        }
    }

    for (uint64_t node = 0; node < curNode; node++)
    {
        const auto &curBuf = mForeignRecvBufs.at(node);
        if (curBuf->try_dequeue(receivedMessage.message))
        {
            receivedMessage.senderId = node;
            return receivedMessage;
        }
    }

    return {};
}

/* This function is used to receive messages from the nodes in own RSM.
 *
 */
pipeline::LocalCrossChainMessage Pipeline::RecvFromOwnRsm()
{
    static uint64_t curNode{0ULL - 1};
    pipeline::LocalCrossChainMessage receivedMessage{};

    curNode = (curNode + 1 == mLocalRecvBufs.size()) ? 0 : curNode + 1;

    for (uint64_t node = curNode; node < mLocalRecvBufs.size(); node++)
    {
        if (node == kOwnConfiguration.kNodeId)
        {
            continue;
        }
        const auto &curBuf = mLocalRecvBufs.at(node);
        if (curBuf->try_dequeue(receivedMessage.message))
        {
            receivedMessage.senderId = node;
            return receivedMessage;
        }
    }

    for (uint64_t node = 0; node < curNode; node++)
    {
        if (node == kOwnConfiguration.kNodeId)
        {
            continue;
        }
        const auto &curBuf = mLocalRecvBufs.at(node);
        if (curBuf->try_dequeue(receivedMessage.message))
        {
            receivedMessage.senderId = node;
            return receivedMessage;
        }
    }

    return {};
}
