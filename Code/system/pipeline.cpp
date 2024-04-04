#include "pipeline.h"

#include "acknowledgment.h"
#include "crypto.h"

#include <algorithm>
#include <boost/container/small_vector.hpp>
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
    const long kNumOfBufferedElements = std::min<long>(64, (double) kDesiredMemoryUsage / kNumSocketsTotal / std::max<long>(250000, PACKET_SIZE));
    addMetric("socket-buffer-size-receive", kNumOfBufferedElements);
    bool nngSetTimeoutResult = nng_socket_set_ms(socket, NNG_OPT_RECVTIMEO, maxNngBlockingTime.count());
    if (nngSetTimeoutResult != 0)
    {
        SPDLOG_CRITICAL("Cannot set timeout for listening url {} Return value {}", url, nng_strerror(nngSetTimeoutResult));
        std::abort();
    }
    bool nngSetSndBufSizeResult = nng_socket_set_int(socket, NNG_OPT_SENDBUF, kNumOfBufferedElements);
    if (nngSetSndBufSizeResult != 0)
    {
        SPDLOG_CRITICAL("Cannot set send buf size {} for url {} RV {}", kNumOfBufferedElements, url, nng_strerror(nngSetSndBufSizeResult));
        std::abort();
    }
    bool nngSetRecBufSizeResult = nng_socket_set_int(socket, NNG_OPT_RECVBUF, kNumOfBufferedElements);
    if (nngSetRecBufSizeResult != 0)
    {
        SPDLOG_CRITICAL("Cannot set rec buf size {} for url {} RV {}", kNumOfBufferedElements, url, nng_strerror(nngSetRecBufSizeResult));
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
    const long kNumOfBufferedElements = std::min<long>(64, (double) kDesiredMemoryUsage / kNumSocketsTotal / std::max<long>(250000, PACKET_SIZE));
    addMetric("socket-buffer-size-send", kNumOfBufferedElements);
    bool nngSetSndBufSizeResult = nng_socket_set_int(socket, NNG_OPT_SENDBUF, kNumOfBufferedElements);
    if (nngSetSndBufSizeResult != 0)
    {
        SPDLOG_CRITICAL("Cannot set send buf size {} for url {} RV {}", kNumOfBufferedElements, url, nng_strerror(nngSetSndBufSizeResult));
        std::abort();
    }
    bool nngSetRecBufSizeResult = nng_socket_set_int(socket, NNG_OPT_RECVBUF, kNumOfBufferedElements);
    if (nngSetRecBufSizeResult != 0)
    {
        SPDLOG_CRITICAL("Cannot set rec buf size {} for url {} RV {}", kNumOfBufferedElements, url, nng_strerror(nngSetRecBufSizeResult));
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
static int sendMessage(const nng_socket &socket, nng_msg *message)
{
    if (message == nullptr) 
    {
        SPDLOG_CRITICAL("MESSAGE IS NULL!");
        return -1;
    }
    const auto sendReturnValue = nng_sendmsg(socket, message, 0);
    const bool isActualError = sendReturnValue != 0 && sendReturnValue != nng_errno_enum::NNG_EAGAIN &&
                               sendReturnValue != nng_errno_enum::NNG_ETIMEDOUT;
    if (isActualError)
    {
        SPDLOG_CRITICAL("nng_send has error value = {}", nng_strerror(sendReturnValue));
    }
    if (sendReturnValue != 0)
    {
        SPDLOG_CRITICAL("NNG SEND MESSAGE RETURN VALUE IS: {}", nng_strerror(sendReturnValue));
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
    static std::string staticData = std::string(get_packet_size(), 'L');
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

void Pipeline::runSendThread(std::string sendUrl, pipeline::MessageQueue<nng_msg *> *const sendBuffer,
                             const uint64_t destNodeId, const bool isLocal)
{
    const auto nodenet = (isLocal) ? get_rsm_id() : get_other_rsm_id();
    SPDLOG_CRITICAL("Sending to [{} : {}] : URL={}", destNodeId, nodenet, sendUrl);

    if ((ALL_TO_ALL || ONE_TO_ONE) && isLocal)
    {
        return;
    }
    if (ONE_TO_ONE && destNodeId != kOwnConfiguration.kNodeId)
    {
        return;
    }

    // If we are running the GEOBFT protocol, and we are marked as sending
    // messages to a remote RSM, we should not send messages unless our 
    // ID is the sending node ID (in this case, this ID 0) 
    if (GEOBFT && !isLocal && kOwnConfiguration.kNodeId != 0) // TODO: Make sender_id a global variable 
    {
        return;
    }

    constexpr auto kNngSendSuccess = 0;

    bindThreadBetweenCpu(5,8);
    nng_socket sendSocket = openSendSocket(sendUrl, kMaxNngBlockingTime);
    bindThreadBetweenCpu(4,4);
    nng_msg *newMessage;
    uint64_t numSent{};

    while (not mShouldThreadStop.load(std::memory_order_relaxed))
    {
        while (true)
        {
            if (sendBuffer->try_dequeue(newMessage))
            {
                break;
            }
            if (mShouldThreadStop.load(std::memory_order_relaxed))
            {
                goto exit;
            }
            std::this_thread::yield();
        }
        
        while (true)
        {
            if (sendMessage(sendSocket, newMessage) == kNngSendSuccess)
            {
                break;
            }
            if (mShouldThreadStop.load(std::memory_order_relaxed))
            {
                nng_msg_free(newMessage);
                goto exit;
            }
            std::this_thread::yield();
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

    if ((ALL_TO_ALL || ONE_TO_ONE) && isLocal)
    {
        return;
    }
    if (ONE_TO_ONE && sendNodeId != kOwnConfiguration.kNodeId)
    {
        return;
    }

    bindThreadBetweenCpu(5,8);
    nng_socket recvSocket = openReceiveSocket(recvUrl, kMaxNngBlockingTime);
    bindThreadBetweenCpu(4,4);
    std::optional<nng_msg *> message;
    uint64_t numRecv{};

    while (not mShouldThreadStop.load(std::memory_order_relaxed))
    {
        while (true)
        {
            if ((message = receiveMessage(recvSocket)).has_value())
            {
                break;
            }
            // if ((message = receiveMessage(recvSocket)).has_value())
            // {
            //     break;
            // }
            if (mShouldThreadStop.load(std::memory_order_relaxed))
            {
                goto exit;
            }
            std::this_thread::yield();
        }

        while (true)
        {
            if (recvBuffer->try_enqueue(*message))
            {
                break;
            }
            // if (recvBuffer->try_enqueue(*message))
            // {
            //     break;
            // }
            if (mShouldThreadStop.load(std::memory_order_relaxed))
            {
                nng_msg_free(*message);
                goto exit;
            }
            std::this_thread::yield();
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
                                    const Acknowledgment *const acknowledgment,
                                    pipeline::MessageQueue<nng_msg *> *const sendingQueue,
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

    nng_msg *batchData = serializeProtobuf(batch->data);
    batch->data.Clear();
    batch->batchSizeEstimate = 0;//kProtobufDefaultSize;

    bool pushFailure{};

    while ((pushFailure = not sendingQueue->try_enqueue(batchData)) && not is_test_over())
        ;

    batch->creationTime = curTime;

    if (pushFailure)
    {
        nng_msg_free(batchData);
    }
}

void Pipeline::flushBufferedFileMessage(pipeline::CrossChainMessageBatch *const batch,
                                        const Acknowledgment *const acknowledgment,
                                        pipeline::MessageQueue<nng_msg *> *const sendingQueue,
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

    nng_msg *batchData = serializeFileProtobuf(batch->data);
    batch->data.Clear();
    batch->batchSizeEstimate = kProtobufDefaultSize;

    bool pushFailure{};

    while ((pushFailure = not sendingQueue->try_enqueue(batchData)) && not is_test_over())
        ;

    batch->creationTime = curTime;

    if (pushFailure)
    {
        nng_msg_free(batchData);
    }
}
bool Pipeline::bufferedMessageSend(scrooge::CrossChainMessageData &&message,
                                   pipeline::CrossChainMessageBatch *const batch,
                                   const Acknowledgment *const acknowledgment,
                                   pipeline::MessageQueue<nng_msg *> *const sendingQueue,
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
                                       pipeline::MessageQueue<nng_msg *> *const sendingQueue,
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

/* This function is used to send message to f+1 nodes in the other RSM. 
 * f+1 nodes are the minimum number of nodes the sending RSM needs to communicate with
 * to maintain correctness while being optimal in GeoBFT. 
 *
 */
void Pipeline::SendToGeoBFTQuorumOtherRsm(scrooge::CrossChainMessageData &&messageData,
                                 std::chrono::steady_clock::time_point curTime)
{
    auto batchCreationTime = &mForeignMessageBatches.front().creationTime;
    auto batch = &mForeignMessageBatches.front().data;
    auto batchSize = &mForeignMessageBatches.front().batchSizeEstimate;

    const auto newDataSize = messageData.ByteSizeLong();
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

    auto foreignAliveNodes = mAliveNodesForeign;
    nng_msg *batchData = serializeProtobuf(*batch);
   
    uint64_t geobft_quorum_counter = 0; // TODO: Potential source of performance degradation
    const uint64_t geobft_quorum_size = (kOwnConfiguration.kOtherNetworkSize - 1)/replication_factor + 1; // TODO: Move this
    while (geobft_quorum_counter < geobft_quorum_size && not is_test_over())
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
        geobft_quorum_counter += 1;
        while (not curBuffer->try_enqueue(curMessage))
        {
            if (is_test_over())
            {
                const bool isLastIteration = geobft_quorum_counter >= geobft_quorum_size;
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
    *batchCreationTime = curTime;
}

// Sends message to a quorum of nodes in a remote RSM
void Pipeline::SendFileToGeoBFTQuorumOtherRsm(scrooge::CrossChainMessageData &&messageData,
                                     std::chrono::steady_clock::time_point curTime)
{
    //SPDLOG_CRITICAL("Beginning of Fil GeoBFT function");
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

    //SPDLOG_CRITICAL("Batch setup complete in File GeoBFT");
    auto foreignAliveNodes = mAliveNodesForeign;
    nng_msg *batchData = serializeFileProtobuf(*batch);
    uint64_t geobft_quorum_counter = 0; // TODO: Potential source of performance degradation
    const uint64_t geobft_quorum_size = (kOwnConfiguration.kOtherNetworkSize - 1)/replication_factor + 1; // TODO: Move this
    while (geobft_quorum_counter < geobft_quorum_size && not is_test_over())
    {
        geobft_quorum_counter += 1;
        const auto curDestination = std::countr_zero(foreignAliveNodes.to_ulong());
        foreignAliveNodes.reset(curDestination);
        const auto &curBuffer = mForeignSendBufs.at(curDestination);
        nng_msg *curMessage = batchData;
        //SPDLOG_CRITICAL("First checks of the foreignAliveNodes variable");
        if (geobft_quorum_counter < geobft_quorum_size)
        {
            nng_msg_dup(&curMessage, batchData);
        }
        else
        {
            curMessage = batchData;
        }
        
        //SPDLOG_CRITICAL("Checkpoint 1 in while loop");
        while (not curBuffer->try_enqueue(curMessage))
        {
            if (is_test_over())
            {
                const bool isLastIteration = geobft_quorum_counter >= geobft_quorum_size;
                if (not isLastIteration)
                {
                    nng_msg_free(batchData);
                }
                nng_msg_free(curMessage);
                SPDLOG_CRITICAL("Breaking out of the loop!");
                break;
            }
        }
        //SPDLOG_CRITICAL("Onto next iteration: {}", geobft_quorum_counter);
    }
    //nng_msg_free(batchData);
    batch->Clear();
    *batchSize = 0;
    *batchCreationTime = curTime;
}

/* This function is used to send message to all nodes in the other RSM.
 *
 */
void Pipeline::SendToAllOtherRsm(scrooge::CrossChainMessageData &&messageData,
                                 std::chrono::steady_clock::time_point curTime)
{
    auto batchCreationTime = &mForeignMessageBatches.front().creationTime;
    auto batch = &mForeignMessageBatches.front().data;
    auto batchSize = &mForeignMessageBatches.front().batchSizeEstimate;

    const auto newDataSize = messageData.ByteSizeLong();
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

    auto foreignAliveNodes = mAliveNodesForeign;
    nng_msg *batchData = serializeProtobuf(*batch);
    
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

        while (not curBuffer->try_enqueue(curMessage))
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
    *batchCreationTime = curTime;
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

    auto foreignAliveNodes = mAliveNodesForeign;
    nng_msg *batchData = serializeFileProtobuf(*batch);

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

        while (not curBuffer->try_enqueue(curMessage))
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
    *batchCreationTime = curTime;
}

bool Pipeline::rebroadcastToOwnRsm(nng_msg *message)
{
    static std::bitset<64> remainingDestinations{};
    static nng_msg *curMessage{};

    const bool isContinuation = remainingDestinations.any();

    if (not isContinuation)
    {
        remainingDestinations = mAliveNodesLocal;
        remainingDestinations.reset(kOwnConfiguration.kNodeId);
        curMessage = nullptr;
    }

    std::bitset<64> failedSends{};
    while (remainingDestinations.any() && not is_test_over())
    {
        const auto curDestination = std::countr_zero(remainingDestinations.to_ulong());
        remainingDestinations.reset(curDestination);
        const auto &curBuffer = mLocalSendBufs.at(curDestination);

        if (curMessage)
        {
            // curMessage is correctly set
        }
        else if (remainingDestinations.any() || failedSends.any())
        {
            //SPDLOG_CRITICAL("REBROADCAST: NNG MSG DUP");
            nng_msg_dup(&curMessage, message);
        }
        else
        {
            //SPDLOG_CRITICAL("REBROADCAST: SET CURR_MSG to MSG");
            curMessage = message;
        }

        if (curBuffer->try_enqueue(curMessage))
        {
            curMessage = nullptr;
        }
        else
        {
            failedSends.set(curDestination);
            //SPDLOG_CRITICAL("Bitset: {}", failedSends.to_string());
            // curMessage <- message that should be used next time
        }
    }
    remainingDestinations = failedSends;

    return remainingDestinations.none();
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
