#include "pipeline.h"

#include "acknowledgment.h"
#include "crypto.h"

#include <boost/container/small_vector.hpp>
#include <nng/protocol/pair1/pair.h>

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
    const auto curAck = acknowledgment.getAckIterator();
    if (!curAck.has_value())
    {
        return;
    }

    message->mutable_ack_count()->set_value(curAck.value());
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

    bool nngSetTimeoutResult = nng_setopt_ms(socket, NNG_OPT_RECVTIMEO, maxNngBlockingTime.count());
    if (nngSetTimeoutResult != 0)
    {
        SPDLOG_CRITICAL("Cannot set timeout for listening Return value {}", url, nng_strerror(nngSetTimeoutResult));
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

    return socket;
}

/* Sending data to nodes of other RSM.
 *
 * @param buf is the outgoing message of protobuf type.
 * @param socket is the nng_socket to put the data into
 */
static int sendMessage(const nng_socket &socket, nng_msg *message)
{
    const auto sendReturnValue = nng_sendmsg(socket, message, 0);
    const bool isActualError = sendReturnValue != 0 && sendReturnValue != nng_errno_enum::NNG_EAGAIN &&
                               sendReturnValue != nng_errno_enum::NNG_ETIMEDOUT;
    if (isActualError)
    {
        SPDLOG_CRITICAL("nng_send has error value = {}", nng_strerror(sendReturnValue));
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

    const auto receiveReturnValue = nng_recvmsg(socket, &msg, 0);

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
    const auto bufferTime = 2s;
    std::this_thread::sleep_until(closeTime + bufferTime);
    nng_close(socket);
}

template <typename T> static nng_msg *serializeProtobuf(const T &proto)
{
    const auto protoSize = proto.ByteSizeLong();
    nng_msg *message;
    const auto allocResult = nng_msg_alloc(&message, protoSize) == 0;
    char *const messageData = (char *)nng_msg_body(message);

    bool success = allocResult && proto.SerializeToArray(messageData, protoSize);

    if (not success)
    {
        SPDLOG_CRITICAL("PROTO SERIALIZE FAILED DataSize={} AllocResult={}", protoSize, allocResult);
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
    mAliveNodesLocal.store(localAliveNodes);
    mAliveNodesForeign.store(foreignAliveNodes);
}

Pipeline::~Pipeline()
{
    const auto joinThread = [](std::thread &thread) { thread.join(); };
    const auto emptyQueue = [](auto &queue) {
        nng_msg *msg;
        while (queue && queue->try_dequeue(msg))
        {
            nng_msg_free(msg);
        }
    };

    mShouldThreadStop = true;
    
    std::for_each(mLocalSendThreads.begin(), mLocalSendThreads.end(), joinThread);
    std::for_each(mForeignSendThreads.begin(), mForeignSendThreads.end(), joinThread);
    std::for_each(mLocalRecvThreads.begin(), mLocalRecvThreads.end(), joinThread);
    std::for_each(mForeignRecvThreads.begin(), mForeignRecvThreads.end(), joinThread);

    std::for_each(mLocalSendBufs.begin(), mLocalSendBufs.end(), emptyQueue);
    std::for_each(mForeignSendBufs.begin(), mForeignSendBufs.end(), emptyQueue);
    std::for_each(mLocalRecvBufs.begin(), mLocalRecvBufs.end(), emptyQueue);
    std::for_each(mForeignRecvBufs.begin(), mForeignRecvBufs.end(), emptyQueue);
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
            continue;
        }

        constexpr bool kIsLocal = true;
        const auto &localUrl = kOwnNetworkUrls.at(localNodeId);
        const auto sendingPort = getSendPort(localNodeId, false);
        const auto receivingPort = getReceivePort(localNodeId, false);

        auto sendingUrl = "tcp://" + localUrl + ":" + std::to_string(sendingPort);
        auto receivingUrl = "tcp://" + kOwnUrl + ":" + std::to_string(receivingPort);

        mLocalSendBufs.push_back(std::make_unique<pipeline::MessageQueue<nng_msg *>>(kBufferSize));
        mLocalRecvBufs.push_back(std::make_unique<pipeline::MessageQueue<nng_msg *>>(kBufferSize));
        mLocalSendThreads.push_back(std::thread(&Pipeline::runSendThread, this, sendingUrl, mLocalSendBufs.back().get(),
                                                localNodeId, kIsLocal));
        mLocalRecvThreads.push_back(std::thread(&Pipeline::runRecvThread, this, receivingUrl,
                                                mLocalRecvBufs.back().get(), localNodeId, kIsLocal));
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

        mForeignSendBufs.push_back(std::make_unique<pipeline::MessageQueue<nng_msg *>>(kBufferSize));
        mForeignRecvBufs.push_back(std::make_unique<pipeline::MessageQueue<nng_msg *>>(kBufferSize));
        mForeignSendThreads.push_back(std::thread(&Pipeline::runSendThread, this, sendingUrl,
                                                  mForeignSendBufs.back().get(), foreignNodeId, kIsLocal));
        mForeignRecvThreads.push_back(std::thread(&Pipeline::runRecvThread, this, receivingUrl,
                                                  mForeignRecvBufs.back().get(), foreignNodeId, kIsLocal));
    }
}

void Pipeline::reportFailedNode(const std::string &nodeUrl, const uint64_t nodeId, const bool isLocal)
{
    const auto ownNodeId = kOwnConfiguration.kNodeId;
    const auto ownNetwork = get_rsm_id();
    const auto failedNetwork = (isLocal) ? get_rsm_id() : get_other_rsm_id();
    SPDLOG_CRITICAL("Node [{} : {}] detected failed Node [{} : {} :: '{}']", ownNodeId, ownNetwork, nodeId,
                    failedNetwork, nodeUrl); // used for detecting configuration issues
    if (isLocal)
    {
        reset_atomic_bit(mAliveNodesLocal, nodeId);
    }
    else
    {
        reset_atomic_bit(mAliveNodesForeign, nodeId);
    }
}

void Pipeline::runSendThread(std::string sendUrl, pipeline::MessageQueue<nng_msg *> *const sendBuffer,
                             const uint64_t destNodeId, const bool isLocal)
{
    const auto nodenet = (isLocal) ? get_rsm_id() : get_other_rsm_id();
    SPDLOG_INFO("Sending to [{} : {}] : URL={}", destNodeId, nodenet, sendUrl);

    constexpr auto kNngSendSuccess = 0;
    constexpr auto kMaxWaitTime = 10s;

    nng_socket sendSocket = openSendSocket(sendUrl, kMaxNngBlockingTime);
    nng_msg *newMessage;
    uint64_t numSent{};
    auto kStartTime = std::chrono::steady_clock::now();

    // Check if socket is being listened to
    while (not sendBuffer->wait_dequeue_timed(newMessage, 500ms))
    {
        if (mShouldThreadStop.load(std::memory_order_relaxed))
        {
            goto exit;
        }
    }

    kStartTime = std::chrono::steady_clock::now();
    while (sendMessage(sendSocket, newMessage) != kNngSendSuccess)
    {
        if (std::chrono::steady_clock::now() - kStartTime > kMaxWaitTime ||
            mShouldThreadStop.load(std::memory_order_relaxed))
        {
            reportFailedNode(sendUrl, destNodeId, isLocal);
            nng_msg_free(newMessage);
            goto exit;
        }
    }
    numSent++;

    // steady state
    while (not mShouldThreadStop.load(std::memory_order_relaxed))
    {
        while (not sendBuffer->wait_dequeue_timed(newMessage, 500ms))
        {
            if (mShouldThreadStop.load(std::memory_order_relaxed))
            {
                goto exit;
            }
        }
        while (sendMessage(sendSocket, newMessage) != kNngSendSuccess)
        {
            if (mShouldThreadStop.load(std::memory_order_relaxed))
            {
                nng_msg_free(newMessage);
                goto exit;
            }
        }
        numSent++;
    }

exit:
    SPDLOG_CRITICAL("Sent {} many to [{} : {}] : URL={}", numSent, destNodeId, nodenet, sendUrl);
    SPDLOG_INFO("Pipeline Sending Thread Exiting");
    const auto finishTime = std::chrono::steady_clock::now();
    closeSocket(sendSocket, finishTime);
}

void Pipeline::runRecvThread(std::string recvUrl, pipeline::MessageQueue<nng_msg *> *const recvBuffer,
                             const uint64_t sendNodeId, const bool isLocal)
{
    const auto nodenet = (isLocal) ? get_rsm_id() : get_other_rsm_id();
    SPDLOG_INFO("Recv from [{} : {}] : URL={}", sendNodeId, nodenet, recvUrl);

    nng_socket recvSocket = openReceiveSocket(recvUrl, kMaxNngBlockingTime);
    std::optional<nng_msg *> message;
    uint64_t numRecv{};

    while (not mShouldThreadStop.load(std::memory_order_relaxed))
    {
        while (not(message = receiveMessage(recvSocket)).has_value())
        {
            if (mShouldThreadStop.load(std::memory_order_relaxed))
            {
                goto exit;
            }
        }

        while (not recvBuffer->wait_enqueue_timed(*message, 500ms))
        {
            if (mShouldThreadStop.load(std::memory_order_relaxed))
            {
                nng_msg_free(*message);
                goto exit;
            }
        }
        numRecv++;
    }

exit:
    SPDLOG_CRITICAL("Recv {} many from [{} : {}] : URL={}", numRecv, sendNodeId, nodenet, recvUrl);
    SPDLOG_INFO("Pipeline Recv Thread Exiting");
    const auto finishTime = std::chrono::steady_clock::now();
    closeSocket(recvSocket, finishTime);
}

void Pipeline::flushBufferedMessage(pipeline::CrossChainMessageBatch *const batch,
                                    const Acknowledgment* const acknowledgment,
                                    pipeline::MessageQueue<nng_msg *> *const sendingQueue)
{
    constexpr auto kSleepTime = 2s;

    if (acknowledgment)
    {
        setAckValue(&batch->data, *acknowledgment);
        generateMessageMac(&batch->data);
    }

    nng_msg* batchData = serializeProtobuf(batch->data);
    batch->data.Clear();
    batch->batchSizeEstimate = kProtobufDefaultSize;

    bool pushFailure{};

    while ((pushFailure = not sendingQueue->wait_enqueue_timed(batchData, kSleepTime)) &&
           not is_test_over())
        ;

    if (pushFailure)
    {
        nng_msg_free(batchData);
    }
}

bool Pipeline::bufferedMessageSend(scrooge::CrossChainMessageData &&message,
                                   pipeline::CrossChainMessageBatch *const batch,
                                   const Acknowledgment * const acknowledgment,
                                   pipeline::MessageQueue<nng_msg *> *const sendingQueue)
{
    batch->batchSizeEstimate += message.ByteSizeLong();
    batch->data.mutable_data()->Add(std::move(message));

    bool shouldSend = batch->batchSizeEstimate >= kMinimumBatchSize;
    if (not shouldSend)
    {
        return false;
    }

    flushBufferedMessage(batch, acknowledgment, sendingQueue);
    return true;
}
/* This function is used to send message to a specific node in other RSM.
 *
 * @param nid is the identifier of the node in the other RSM.
 */
bool Pipeline::SendToOtherRsm(uint64_t receivingNodeId, scrooge::CrossChainMessageData &&messageData, const Acknowledgment * const acknowledgment)
{
    SPDLOG_DEBUG("Queueing Send message to other RSM: nodeId = {}, message = [SequenceId={}, AckId={}, size='{}']",
                 receivingNodeId, message.data().sequence_number(), getLogAck(message),
                 message.data().message_content().size());

    const auto &destinationBuffer = mForeignSendBufs.at(receivingNodeId);
    const auto destinationBatch = mForeignMessageBatches.data() + receivingNodeId;

    if (messageData.message_content().size())
    {
        return bufferedMessageSend(std::move(messageData), destinationBatch, acknowledgment, destinationBuffer.get());
    }
    else
    {
        flushBufferedMessage(destinationBatch, acknowledgment, destinationBuffer.get());
        return true;
    }
}

/* This function is used to send message to a specific node in other RSM.
 *
 * @param nid is the identifier of the node in the other RSM.
 */
void Pipeline::SendToAllOtherRsm(scrooge::CrossChainMessageData &&messageData)
{
    constexpr auto kSleepTime = 2s;

    auto batch = &mForeignMessageBatches.front().data;
    auto batchSize = &mForeignMessageBatches.front().batchSizeEstimate;

    const auto newDataSize = messageData.ByteSizeLong();
    batch->mutable_data()->Add(std::move(messageData));
    *batchSize += newDataSize;

    bool shouldSend = *batchSize >= kMinimumBatchSize;
    if (not shouldSend)
    {
        return;
    }

    auto foreignAliveNodes = mAliveNodesForeign.load(std::memory_order_relaxed);
    nng_msg* batchData = serializeProtobuf(*batch);

    while (foreignAliveNodes.any() && not is_test_over())
    {
        const auto curDestination = std::countr_zero(foreignAliveNodes.to_ulong());
        foreignAliveNodes.reset(curDestination);
        const auto &curBuffer = mForeignSendBufs.at(curDestination);
        nng_msg *curMessage;

        if (foreignAliveNodes.any())
        {
            nng_msg_dup(&curMessage, batchData);
        }
        else
        {
            curMessage = batchData;
        }

        while (not curBuffer->wait_enqueue_timed(curMessage, kSleepTime))
        {
            if (is_test_over())
            {
                const bool isLastIteration = foreignAliveNodes.none();
                if (not isLastIteration)
                {
                    nng_msg_free(batchData);
                }
                nng_msg_free(curMessage);

                break;
            }
        }
    }
    batch->Clear();
    *batchSize = 0;
}

bool Pipeline::rebroadcastToOwnRsm(nng_msg *message)
{
    static std::optional<uint64_t> lastSentNode{};
    static nng_msg *curMessage{};
    auto remainingDestinations = mAliveNodesLocal.load(std::memory_order_relaxed);
    remainingDestinations.reset(kOwnConfiguration.kNodeId);

    if (lastSentNode.has_value())
    {
        remainingDestinations &= (~0) << lastSentNode.value();
    }

    while (remainingDestinations.any() && not is_test_over())
    {
        const auto curDestination = std::countr_zero(remainingDestinations.to_ulong());
        remainingDestinations.reset(curDestination);
        const auto &curBuffer = mLocalSendBufs.at(curDestination);

        if (remainingDestinations.any() && not lastSentNode.has_value())
        {
            nng_msg_dup(&curMessage, message);
        }
        else if (not lastSentNode.has_value())
        {
            curMessage = message;
        }
        lastSentNode.reset();

        if (not curBuffer->try_enqueue(curMessage))
        {
            lastSentNode = curDestination;
            // will possibly leak one message on shutdown /shrug
            return false;
        }
    }
    lastSentNode.reset();
    return true;
}

/* This function is used to receive messages from the other RSM.
 *
 */
pipeline::ReceivedCrossChainMessage Pipeline::RecvFromOtherRsm()
{
    static uint64_t curNode{0ULL - 1};
    nng_msg *message;

    curNode = (curNode + 1 == mForeignRecvBufs.size()) ? 0 : curNode + 1;

    for (uint64_t node = curNode; node < mForeignRecvBufs.size(); node++)
    {
        const auto &curBuf = mForeignRecvBufs.at(node);
        if (curBuf->try_dequeue(message))
        {
            return pipeline::ReceivedCrossChainMessage{.message = message, .senderId = node};
        }
    }

    for (uint64_t node = 0; node < curNode; node++)
    {
        const auto &curBuf = mForeignRecvBufs.at(node);
        if (curBuf->try_dequeue(message))
        {
            return pipeline::ReceivedCrossChainMessage{.message = message, .senderId = node};
        }
    }

    return {};
}

/* This function is used to receive messages from the nodes in own RSM.
 *
 */
pipeline::ReceivedCrossChainMessage Pipeline::RecvFromOwnRsm()
{
    static uint64_t curNode{0ULL - 1};
    nng_msg *message;

    curNode = (curNode + 1 == mLocalRecvBufs.size()) ? 0 : curNode + 1;

    for (uint64_t node = curNode; node < mLocalRecvBufs.size(); node++)
    {
        if (node == kOwnConfiguration.kNodeId)
        {
            continue;
        }
        const auto &curBuf = mLocalRecvBufs.at(node);
        if (curBuf->try_dequeue(message))
        {
            return pipeline::ReceivedCrossChainMessage{.message = message, .senderId = node};
        }
    }

    for (uint64_t node = 0; node < curNode; node++)
    {
        if (node == kOwnConfiguration.kNodeId)
        {
            continue;
        }
        const auto &curBuf = mLocalRecvBufs.at(node);
        if (curBuf->try_dequeue(message))
        {
            return pipeline::ReceivedCrossChainMessage{.message = message, .senderId = node};
        }
    }

    return {};
}
