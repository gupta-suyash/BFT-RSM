#include "pipeline.h"

#include "acknowledgment.h"
#include "crypto.h"

#include <boost/container/small_vector.hpp>

static int64_t getLogAck(const scrooge::CrossChainMessage &message)
{
    if (!message.has_ack_count())
    {
        return -1;
    }
    return message.ack_count().value();
}

template<typename atomic_bitset>
void reset_atomic_bit(atomic_bitset& set, uint64_t bit)
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
    const auto openResult = nng_pull0_open(&socket);
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
        SPDLOG_CRITICAL("Cannot set timeout for listening Return value {}", url,
                        nng_strerror(nngSetTimeoutResult));
        std::abort();
    }

    return socket;
}

static nng_socket openSendSocket(const std::string &url, std::chrono::milliseconds maxNngBlockingTime)
{
    constexpr auto kSuccess = 0;

    nng_socket socket;
    const auto openResult = nng_push0_open(&socket);

    const bool cannotOpenSocket = kSuccess != openResult;
    if (cannotOpenSocket)
    {
        SPDLOG_CRITICAL("Cannot open socket for sending on URL {} ERROR: {}", url, nng_strerror(openResult));
        std::abort();
    }

    const auto nngDialResult = nng_dial(socket, url.c_str(), nullptr, NNG_FLAG_NONBLOCK);
    
    if (nngDialResult != kSuccess)
    {
        SPDLOG_CRITICAL("CANNOT OPEN SOCKET FOR SENDING. URL = '{}' Return value {}", url,
                        nng_strerror(nngDialResult));
        std::abort();
    }

    bool nngSetTimeoutResult = nng_setopt_ms(socket, NNG_OPT_SENDTIMEO, maxNngBlockingTime.count());
    if (nngSetTimeoutResult != kSuccess)
    {
        SPDLOG_CRITICAL("Cannot set timeout for sending Return value {}", url,
                        nng_strerror(nngSetTimeoutResult));
        std::abort();
    }

    return socket;
}

/* Sending data to nodes of other RSM.
 *
 * @param buf is the outgoing message of protobuf type.
 * @param socket is the nng_socket to put the data into
 */
static int sendMessage(const nng_socket &socket, nng_msg* message)
{
    const auto sendReturnValue =
        nng_sendmsg(socket, message, 0);
    const bool isActualError = sendReturnValue != 0 && sendReturnValue != nng_errno_enum::NNG_EAGAIN && sendReturnValue != nng_errno_enum::NNG_ETIMEDOUT;
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
static std::optional<nng_msg*> receiveMessage(const nng_socket &socket)
{
    nng_msg* msg;

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

static void closeSocket(nng_socket& socket, std::chrono::steady_clock::time_point closeTime)
{
    // NNG says to wait before closing
    // https://nng.nanomsg.org/man/tip/nng_close.3.html
    const auto bufferTime = 2s;
    std::this_thread::sleep_until(closeTime + bufferTime);
    nng_close(socket);
}

template <typename T> static nng_msg* serializeProtobuf(const T& proto)
{
    const auto protoSize = proto.ByteSizeLong();
    nng_msg* message;
    const auto allocResult = nng_msg_alloc(&message, protoSize) == 0;
    char* const messageData =(char*) nng_msg_body(message);

    bool success = allocResult && proto.SerializeToArray(messageData, protoSize);

    if (not success)
    {
        SPDLOG_CRITICAL("PROTO SERIALIZE FAILED DataSize={} AllocResult={}", protoSize, allocResult);
        std::abort();
    }

    return message;
}

template <typename T> static boost::container::small_vector<nng_msg*, 7> serializeProtobufRepeated(const T& proto, uint64_t numCopies)
{
    boost::container::small_vector<nng_msg*, 7> messages(numCopies);
    messages.front() = serializeProtobuf(proto);
    for (int i = 1; i < numCopies; i++)
    {
        nng_msg_dup(&messages.at(i), messages.front());
    }
    return messages;
}

template <typename T> static T deserializeProtobuf(nng_msg* message)
{
    T proto;
    const auto messageData = nng_msg_body(message);
    const auto messageSize = nng_msg_len(message);
    bool success = proto.ParseFromArray(messageData, messageSize);
    nng_msg_free(message);
    if (not success)
    {
        SPDLOG_CRITICAL("PROTO DESERIALIZE FAILED DataSize={}", messageData);
        std::abort();
    }

    return proto;
}

template <typename T> static T unsafeDeserializeProtobuf(nng_msg* message)
{
    T proto;
    const auto messageData = nng_msg_body(message);
    const auto messageSize = nng_msg_len(message);
    bool success = proto.ParseFromArray(messageData, messageSize);
    // nng_msg_free(message); DOESN"T FREE MESSAGE
    if (not success)
    {
        SPDLOG_CRITICAL("PROTO DESERIALIZE FAILED DataSize={}", messageData);
        std::abort();
    }

    return proto;
}

Pipeline::Pipeline(const std::vector<std::string> &ownNetworkUrls, const std::vector<std::string> &otherNetworkUrls,
                   NodeConfiguration ownConfiguration)
    : kOwnConfiguration(ownConfiguration), kOwnNetworkUrls(ownNetworkUrls), kOtherNetworkUrls(otherNetworkUrls)
{
    std::bitset<64> foreignAliveNodes{}, localAliveNodes{};
    localAliveNodes |= -1ULL ^ (-1ULL << kOwnConfiguration.kOwnNetworkSize);
    foreignAliveNodes |= -1ULL ^ (-1ULL << kOwnConfiguration.kOtherNetworkSize);
    mAliveNodesLocal.store(localAliveNodes);
    mAliveNodesForeign.store(foreignAliveNodes);
}

Pipeline::~Pipeline()
{
    const auto joinThread = [](std::thread& thread) {
        thread.join();
    };
    const auto emptyQueue = [](auto& queue) {
        nng_msg* msg;
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

        mLocalSendBufs.push_back(std::make_unique<pipeline::MessageQueue<nng_msg*>>(kBufferSize));
        mLocalRecvBufs.push_back(std::make_unique<pipeline::MessageQueue<nng_msg*>>(kBufferSize));
        mLocalSendThreads.push_back(std::thread(&Pipeline::runSendThread, this, sendingUrl, mLocalSendBufs.back().get(), localNodeId, kIsLocal));
        mLocalRecvThreads.push_back(std::thread(&Pipeline::runRecvThread, this, receivingUrl, mLocalRecvBufs.back().get(), localNodeId, kIsLocal));
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

        mForeignSendBufs.push_back(std::make_unique<pipeline::MessageQueue<nng_msg*>>(kBufferSize));
        mForeignRecvBufs.push_back(std::make_unique<pipeline::MessageQueue<nng_msg*>>(kBufferSize));
        mForeignSendThreads.push_back(std::thread(&Pipeline::runSendThread, this, sendingUrl, mForeignSendBufs.back().get(), foreignNodeId, kIsLocal));
        mForeignRecvThreads.push_back(std::thread(&Pipeline::runRecvThread, this, receivingUrl, mForeignRecvBufs.back().get(), foreignNodeId, kIsLocal));
    }
}

void Pipeline::reportFailedNode(const std::string& nodeUrl, const uint64_t nodeId, const bool isLocal)
{
    const auto ownNodeId = kOwnConfiguration.kNodeId;
    const auto ownNetwork = get_rsm_id();
    const auto failedNetwork = (isLocal)? get_rsm_id() : get_other_rsm_id();
    SPDLOG_CRITICAL("Node [{} : {}] detected failed Node [{} : {} :: '{}']", ownNodeId, ownNetwork, nodeId, failedNetwork, nodeUrl);
    if (isLocal)
    {
        reset_atomic_bit(mAliveNodesLocal, nodeId);
    }
    else
    {
        reset_atomic_bit(mAliveNodesForeign, nodeId);
    }
}

void Pipeline::runSendThread(std::string sendUrl, pipeline::MessageQueue<nng_msg*>* const sendBuffer, const uint64_t destNodeId, const bool isLocal)
{
    const auto nodenet = (isLocal)? get_rsm_id() : get_other_rsm_id();
    SPDLOG_INFO("Sending to [{} : {}] : URL={}", destNodeId, nodenet, sendUrl);

    constexpr auto kNngSendSuccess = 0;
    constexpr auto kMaxWaitTime = 3s;

    nng_socket sendSocket = openSendSocket(sendUrl, kMaxNngBlockingTime);
    nng_msg* newMessage;
    uint64_t numSent{};
    auto kStartTime = std::chrono::steady_clock::now();

    // Check if socket is being listened to
    while(not sendBuffer->wait_dequeue_timed(newMessage, 500ms))
    {
        if (mShouldThreadStop.load(std::memory_order_relaxed))
        {
            goto exit;
        }
    }

    kStartTime = std::chrono::steady_clock::now();
    while(sendMessage(sendSocket, newMessage) != kNngSendSuccess)
    {
        if (std::chrono::steady_clock::now() - kStartTime > kMaxWaitTime || mShouldThreadStop.load(std::memory_order_relaxed))
        {
            nng_msg_free(newMessage);
            reportFailedNode(sendUrl, destNodeId, isLocal);
            goto exit;
        }
    }
    numSent++;

    // steady state
    while (not mShouldThreadStop.load(std::memory_order_relaxed))
    {
        while(not sendBuffer->wait_dequeue_timed(newMessage, 500ms))
        {
            if (mShouldThreadStop.load(std::memory_order_relaxed))
            {
                goto exit;
            }
        }
        while(sendMessage(sendSocket, newMessage) != kNngSendSuccess)
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

void Pipeline::runRecvThread(std::string recvUrl, pipeline::MessageQueue<nng_msg*>* const recvBuffer, const uint64_t sendNodeId, const bool isLocal)
{
    const auto nodenet = (isLocal)? get_rsm_id() : get_other_rsm_id();
    SPDLOG_INFO("Recv from [{} : {}] : URL={}", sendNodeId, nodenet, recvUrl);

    nng_socket recvSocket = openReceiveSocket(recvUrl, kMaxNngBlockingTime);
    std::optional<nng_msg*> message;
    uint64_t numRecv{};
    
    while (not mShouldThreadStop.load(std::memory_order_relaxed))
    {
        while(not (message = receiveMessage(recvSocket)).has_value())
        {
            if (mShouldThreadStop.load(std::memory_order_relaxed))
            {
                goto exit;
            }
        }

        while(not recvBuffer->wait_enqueue_timed(*message, 500ms))
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

/* This function is used to send message to a specific node in other RSM.
 *
 * @param nid is the identifier of the node in the other RSM.
 */
void Pipeline::SendToOtherRsm(const uint64_t receivingNodeId, scrooge::CrossChainMessage &message)
{
    constexpr auto kSleepTime = 2s;

    SPDLOG_DEBUG("Queueing Send message to other RSM: nodeId = {}, message = [SequenceId={}, AckId={}, size='{}']",
                 receivingNodeId, message.data().sequence_number(), getLogAck(message),
                 message.data().message_content().size());

    const auto isDestFailed = not mAliveNodesForeign.load(std::memory_order_relaxed).test(receivingNodeId);
    if (isDestFailed)
    {
        return;
    }

    // MAC signing TODO
    const auto mdata = message.data();
    const auto mstr = mdata.SerializeAsString();
    // Sign with own key.
    std::string encoded = CmacSignString(get_priv_key(), mstr);
    message.set_validity_proof(encoded);
    // std::cout << "Mac: " << encoded << std::endl;

    const auto messageData = serializeProtobuf(message);
    const auto& destinationBuffer =  mForeignSendBufs.at(receivingNodeId);
    bool isPushFail{};

    while ((isPushFail = not destinationBuffer->wait_enqueue_timed(messageData, kSleepTime)) && not is_test_over())
    {
        const auto isDestFailed = not mAliveNodesForeign.load(std::memory_order_relaxed).test(receivingNodeId);
        if (isDestFailed)
        {
            break;
        }
    }

    if (isPushFail)
    {
        nng_msg_free(messageData);
    }
}

/* This function is used to send message to a specific node in other RSM.
 *
 * @param nid is the identifier of the node in the other RSM.
 */
void Pipeline::SendToAllOtherRsm(const scrooge::CrossChainMessage &message)
{
    constexpr auto kSleepTime = 1s;

    auto remainingDestinations = mAliveNodesForeign.load(std::memory_order_relaxed);
    const auto messages = serializeProtobufRepeated(message, remainingDestinations.count());

    uint64_t msgNumber = 0;
    while (remainingDestinations.any())
    {
        const auto curDestination = std::countr_zero(remainingDestinations.to_ulong());
        remainingDestinations.reset(curDestination);
        const auto& curBuffer =  mForeignSendBufs.at(curDestination);
        const auto& curMessage = messages.at(msgNumber++);
        bool isPushFail{};

        while ((isPushFail = not curBuffer->wait_enqueue_timed(curMessage, kSleepTime)) && not is_test_over())
        {
            const auto isDestFailed = not mAliveNodesForeign.load(std::memory_order_relaxed).test(curDestination);
            if (isDestFailed)
            {
                break;
            }
        }

        if (isPushFail)
        {
            nng_msg_free(curMessage);
        }
    }
}


void Pipeline::BroadcastToOwnRsm(const scrooge::CrossChainMessage &message)
{
    constexpr auto kSleepTime = 1s;

    auto remainingDestinations = mAliveNodesLocal.load(std::memory_order_relaxed);
    remainingDestinations.reset(kOwnConfiguration.kNodeId);
    const auto messages = serializeProtobufRepeated(message, remainingDestinations.count());

    uint64_t msgNumber = 0;
    while (remainingDestinations.any())
    {
        const auto curDestination = std::countr_zero(remainingDestinations.to_ulong());
        remainingDestinations.reset(curDestination);
        const auto& curBuffer =  mLocalSendBufs.at(curDestination);
        const auto& curMessage = messages.at(msgNumber++);
        bool isPushFail{};

        while ((isPushFail = not curBuffer->wait_enqueue_timed(curMessage, kSleepTime)) && not is_test_over())
        {
            const auto isDestFailed = not mAliveNodesLocal.load(std::memory_order_relaxed).test(curDestination);
            if (isDestFailed)
            {
                break;
            }
        }

        if (isPushFail)
        {
            nng_msg_free(curMessage);
        }
    }
}

void Pipeline::rebroadcastToOwnRsm(nng_msg* message)
{
    constexpr auto kSleepTime = 1s;

    auto remainingDestinations = mAliveNodesLocal.load(std::memory_order_relaxed);
    remainingDestinations.reset(kOwnConfiguration.kNodeId);

    while (remainingDestinations.any())
    {
        const auto curDestination = std::countr_zero(remainingDestinations.to_ulong());
        remainingDestinations.reset(curDestination);
        const auto& curBuffer =  mLocalSendBufs.at(curDestination);
        nng_msg* curMessage;
        if (remainingDestinations.any())
        {
            nng_msg_dup(&curMessage, message);
        }
        else
        {
            curMessage = message;
        }
        bool isPushFail{};

        while ((isPushFail = not curBuffer->wait_enqueue_timed(curMessage, kSleepTime)) && not is_test_over())
        {
            const auto isDestFailed = not mAliveNodesLocal.load(std::memory_order_relaxed).test(curDestination);
            if (isDestFailed)
            {
                break;
            }
        }

        if (isPushFail)
        {
            nng_msg_free(curMessage);
        }
    }
}

/* This function is used to receive messages from the other RSM.
 *
 */
void Pipeline::RecvFromOtherRsm(boost::circular_buffer<pipeline::ReceivedCrossChainMessage> &out)
{
    constexpr auto kMaxMsgsPerNode = 5;
    nng_msg* nng_message;
    for (uint64_t node = 0; node < kOwnConfiguration.kOtherNetworkSize && not out.full(); node++)
    {
        const auto& curRecvBuffer = mForeignRecvBufs.at(node);
        for (uint64_t msg = 0; msg < kMaxMsgsPerNode; msg++)
        {
            if (not out.full() && curRecvBuffer->try_dequeue(nng_message))
            {
                auto scroogeMessage = unsafeDeserializeProtobuf<scrooge::CrossChainMessage>(nng_message);
                // Verification TODO
                const auto mdata = scroogeMessage.data();
                const auto mstr = mdata.SerializeAsString();
                // Fetch the sender key
                // const auto senderKey = get_other_rsm_key(nng_message.senderId);
                const auto senderKey = get_priv_key();
                // Verify the message
                bool res = CmacVerifyString(senderKey, mstr, scroogeMessage.validity_proof());
                if (!res)
                {
                    SPDLOG_CRITICAL("NODE {} COULD NOT VERIFY MESSAGE FROM NODE {}", kOwnConfiguration.kNodeId, node);
                }

                if (scroogeMessage.data().sequence_number() % 4 != node)
                    SPDLOG_CRITICAL("GOT RESEND OF MSG {} FROM {}", scroogeMessage.data().sequence_number(), node);
                out.push_back(pipeline::ReceivedCrossChainMessage{std::move(scroogeMessage), node, nng_message});
            }
            else
            {
                break;
            }
        }
    }
}

/* This function is used to receive messages from the nodes in own RSM.
 *
 */
void Pipeline::RecvFromOwnRsm(boost::circular_buffer<scrooge::CrossChainMessage> &out)
{
    constexpr auto kMaxMsgsPerNode = 5;
    nng_msg* nng_message;
    for (uint64_t node = 0; node < mLocalRecvBufs.size() && not out.full(); node++)
    {
        if (node == kOwnConfiguration.kNodeId)
        {
            continue;
        }
        const auto& curRecvBuffer = mLocalRecvBufs.at(node);
        for (uint64_t msg = 0; msg < kMaxMsgsPerNode; msg++)
        {
            if (not out.full() && curRecvBuffer->try_dequeue(nng_message))
            {
                auto scroogeMessage = deserializeProtobuf<scrooge::CrossChainMessage>(nng_message);

                out.push_back(std::move(scroogeMessage));
            }
            else
            {
                break;
            }
        }   
    }
}

/* This function will send keys to own RSM.
 */
void Pipeline::BroadcastKeyToOwnRsm()
{
    /*
    scrooge::KeyExchangeMessage msg;
    msg.clear_node_key();
    msg.clear_node_id();
    msg.set_node_key(get_priv_key());
    msg.set_node_id(kOwnConfiguration.kNodeId);

    constexpr auto kSleepTime = 1ns;

    const auto messages = serializeProtobufRepeated(msg, kOwnConfiguration.kOwnNetworkSize);
    for (int i = 0; i < kOwnConfiguration.kOwnNetworkSize; i++)
    {
        const auto& curBuffer =  mLocalSendBufs.at(i);
        const auto& curMessage = messages.at(i);
        while (not is_test_over() && not curBuffer->wait_enqueue_timed(curMessage, kSleepTime));
    }
    */
}

/* This function will send keys to other RSM.
 */
void Pipeline::BroadcastKeyToOtherRsm()
{
    /*
    scrooge::KeyExchangeMessage msg;
    msg.set_node_key(get_priv_key());
    msg.set_node_id(kOwnConfiguration.kNodeId);

    constexpr auto kSleepTime = 1ns;

    const auto messages = serializeProtobufRepeated(msg, kOwnConfiguration.kOtherNetworkSize);
    for (int i = 0; i < kOwnConfiguration.kOtherNetworkSize; i++)
    {
        const auto& curBuffer =  mForeignSendBufs.at(i);
        const auto& curMessage = messages.at(i);
        while (not is_test_over() && not curBuffer->wait_enqueue_timed(curMessage, kSleepTime));
    }
    */
}

/* This function is used to receive messages from the nodes in own RSM.
 */
void Pipeline::RecvFromOwnRsm()
{
    /*
    const auto ownId = kOwnConfiguration.kNodeId;
    const auto localNetworkSize = kOwnConfiguration.kOwnNetworkSize;

    uint64_t count = 0;
    nng_msg nng_message;
    while (count < (localNetworkSize - 1))
    {
        mLocalReceivedMessageQueue.wait_dequeue(nng_message);
        auto keyMsg = deserializeProtobuf<scrooge::KeyExchangeMessage>(nng_message);
        auto nodeId = keyMsg.node_id();
        auto privKey = keyMsg.node_key();
        set_own_rsm_key(nodeId, privKey);
        std::cout << "Node: " << ownId << " :Key for node: " << nodeId << ": " << privKey << std::endl;
        count++;
    }
    */
}

/* This function is used to receive messages from the other RSM.
 */
void Pipeline::RecvFromOtherRsm()
{
    /*
    const auto foreignNetworkSize = kOwnConfiguration.kOtherNetworkSize;

    uint64_t count = 0;
    pipeline::foreign_nng_message nng_message;
    while (count < foreignNetworkSize)
    {
        if (mForeignReceivedMessageQueue.try_dequeue(nng_message))
        {
            auto keyMsg = deserializeProtobuf<scrooge::KeyExchangeMessage>(nng_message.message);
            auto nodeId = keyMsg.node_id();
            auto privKey = keyMsg.node_key();
            set_other_rsm_key(nodeId, privKey);
            count++;
        }
    }
    */
}
