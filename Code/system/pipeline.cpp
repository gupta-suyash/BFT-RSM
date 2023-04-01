#include "pipeline.h"

#include "acknowledgment.h"
#include "crypto.h"

#include <bit>
#include <list>

static int64_t getLogAck(const scrooge::CrossChainMessage &message)
{
    if (!message.has_ack_count())
    {
        return -1;
    }
    return message.ack_count().value();
}

static nng_socket openReceiveSocket(const std::string &url)
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

    return socket;
}

static nng_socket openSendSocket(const std::string &url)
{
    constexpr auto kResultOpenSuccessful = 0;

    nng_socket socket;
    const auto openResult = nng_push0_open(&socket);

    const bool cannotOpenSocket = kResultOpenSuccessful != openResult;
    if (cannotOpenSocket)
    {
        SPDLOG_CRITICAL("Cannot open socket for sending on URL {} ERROR: {}", url, nng_strerror(openResult));
        std::abort();
    }

    // Asynchronous wait for someone to listen to this socket.
    nng_dial(socket, url.c_str(), nullptr, NNG_FLAG_NONBLOCK);

    return socket;
}

/* Sending data to nodes of other RSM.
 *
 * @param buf is the outgoing message of protobuf type.
 * @param socket is the nng_socket to put the data into
 */
static int sendMessage(const nng_socket &socket, const std::string &message)
{
    const auto sendReturnValue =
        nng_send(socket, const_cast<char *>(message.c_str()), message.size(), NNG_FLAG_NONBLOCK);
    const bool isActualError = sendReturnValue != 0 && sendReturnValue != nng_errno_enum::NNG_EAGAIN;
    if (isActualError)
    {
        SPDLOG_CRITICAL("nng_send has error value = {}", nng_strerror(sendReturnValue));
    }

    return sendReturnValue;
}

/* Uses NNG to attempt nonblocking sends of the message to all set destinations in the socketList
 *
 * @return a DestinationSet holding all destinations which this method failed to deliver to
 */
static pipeline::SendMessageRequest::DestinationSet forwardMessageRequest(
    const pipeline::SendMessageRequest &sendMessageRequest, const std::vector<nng_socket> &sockets)
{
    const auto &[kRequestCreationTime, destinationNodes, messageData] = sendMessageRequest;

    pipeline::SendMessageRequest::DestinationSet failedSends{};
    auto remainingDestinations = destinationNodes;
    while (remainingDestinations.any())
    {
        const auto curDestination = std::countr_zero(remainingDestinations.to_ulong());
        remainingDestinations.reset(curDestination);

        const auto socket = sockets.at(curDestination);

        // send the data
        const auto sendMessageResult = sendMessage(socket, messageData);

        const bool isSendFailure = sendMessageResult != 0;
        if (isSendFailure)
        {
            failedSends.set(curDestination);
            const auto isRealError = sendMessageResult != nng_errno_enum::NNG_EAGAIN;
            if (isRealError)
            {
                SPDLOG_CRITICAL("NNG_SEND error when sending to node {} Error: {}", curDestination,
                                nng_strerror(sendMessageResult));
            }
        }
    }
    return failedSends;
}

/* Receving data from nodes of other RSM.
 *
 * @param socket is the nng_socket to check for data on.
 * @return the protobuf if data for one was contained in the socket.
 */
static std::optional<pipeline::nng_message> receiveMessage(const nng_socket &socket)
{
    char *data;
    size_t dataSize;

    const auto receiveReturnValue = nng_recv(socket, &data, &dataSize, NNG_FLAG_ALLOC | NNG_FLAG_NONBLOCK);

    if (receiveReturnValue != 0)
    {
        if (receiveReturnValue != nng_errno_enum::NNG_EAGAIN)
        {
            SPDLOG_CRITICAL("nng_recv has error value = {}", nng_strerror(receiveReturnValue));
        }

        return std::nullopt;
    }

    return pipeline::nng_message{.dataSize = dataSize, .data = data};
}

static void closeSockets(std::vector<nng_socket> &sockets, std::chrono::steady_clock::time_point closeTime)
{
    // NNG says to wait before closing
    // https://nng.nanomsg.org/man/tip/nng_close.3.html
    const auto bufferTime = 2s;
    std::this_thread::sleep_until(closeTime + bufferTime);

    for (auto &socket : sockets)
    {
        nng_close(socket);
    }
}

static void free_nng_message(pipeline::nng_message message)
{
    nng_free(message.data, message.dataSize);
}

template <typename T> static T deserializeProtobuf(pipeline::nng_message message)
{
    T proto;
    bool success = proto.ParseFromArray(message.data, message.dataSize);
    free_nng_message(message);
    if (not success)
    {
        SPDLOG_CRITICAL("PROTO DESERIALIZE FAILED DataSize={}", message.dataSize);
        std::abort();
    }

    return proto;
}

Pipeline::Pipeline(const std::vector<std::string> &ownNetworkUrls, const std::vector<std::string> &otherNetworkUrls,
                   NodeConfiguration ownConfiguration)
    : kOwnConfiguration(ownConfiguration), kOwnNetworkUrls(ownNetworkUrls), kOtherNetworkUrls(otherNetworkUrls)
{
}

Pipeline::~Pipeline()
{
    mShouldThreadStop = true;
    mForeignMessageSendThread.join();
    mForeignMessageReceiveThread.join();
    mLocalMessageSendThread.join();
    mLocalMessageReceiveThread.join();

    pipeline::foreign_nng_message foreignMessage;
    while (mForeignReceivedMessageQueue.try_dequeue(foreignMessage))
    {
        free_nng_message(foreignMessage.message);
    }

    pipeline::nng_message message;
    while (mLocalReceivedMessageQueue.try_dequeue(message))
    {
        free_nng_message(message);
    }
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
    auto foreignSendSockets = std::make_unique<std::vector<nng_socket>>();
    auto localSendSockets = std::make_unique<std::vector<nng_socket>>();
    auto foreignReceiveSockets = std::make_unique<std::vector<nng_socket>>();
    auto localReceiveSockets = std::make_unique<std::vector<nng_socket>>();

    SPDLOG_INFO("Configuring local sockets: {}", kOwnUrl);
    for (size_t localNodeId = 0; localNodeId < kOwnNetworkUrls.size(); localNodeId++)
    {
        if (kOwnConfiguration.kNodeId == localNodeId)
        {
            localSendSockets->emplace_back();
            localReceiveSockets->emplace_back();
            continue;
        }

        const auto &localUrl = kOwnNetworkUrls.at(localNodeId);
        const auto sendingPort = getSendPort(localNodeId, false);
        const auto receivingPort = getReceivePort(localNodeId, false);

        auto sendingUrl = "tcp://" + localUrl + ":" + std::to_string(sendingPort);
        auto receivingUrl = "tcp://" + kOwnUrl + ":" + std::to_string(receivingPort);

        localSendSockets->emplace_back(openSendSocket(sendingUrl));
        localReceiveSockets->emplace_back(openReceiveSocket(receivingUrl));
    }

    SPDLOG_INFO("Configuring foreign sockets");
    for (size_t foreignNodeId = 0; foreignNodeId < kOtherNetworkUrls.size(); foreignNodeId++)
    {
        const auto &foreignUrl = kOtherNetworkUrls.at(foreignNodeId);
        const auto sendingPort = getSendPort(foreignNodeId, true);
        const auto receivingPort = getReceivePort(foreignNodeId, true);

        auto sendingUrl = "tcp://" + foreignUrl + ":" + std::to_string(sendingPort);
        auto receivingUrl = "tcp://" + kOwnUrl + ":" + std::to_string(receivingPort);

        foreignSendSockets->emplace_back(openSendSocket(sendingUrl));
        foreignReceiveSockets->emplace_back(openReceiveSocket(receivingUrl));
    }

    mForeignMessageSendThread = std::thread(&Pipeline::runForeignSendThread, this, std::move(foreignSendSockets));
    mForeignMessageReceiveThread =
        std::thread(&Pipeline::runForeignReceiveThread, this, std::move(foreignReceiveSockets));
    mLocalMessageSendThread = std::thread(&Pipeline::runLocalSendThread, this, std::move(localSendSockets));
    mLocalMessageReceiveThread = std::thread(&Pipeline::runLocalReceiveThread, this, std::move(localReceiveSockets));
}

void Pipeline::runLocalSendThread(std::unique_ptr<std::vector<nng_socket>> localSendSockets)
{
    constexpr auto kMaxMessageRetryTime = 1s;

    bindThreadToCpu(1);

    std::list<pipeline::SendMessageRequest> messageRequests;

    SPDLOG_INFO("Pipeline Sending Thread Starting");
    while (not mShouldThreadStop.load(std::memory_order_relaxed))
    {
        pipeline::SendMessageRequest newMessageRequest;
        const auto curTime = std::chrono::steady_clock::now();

        while (std::chrono::steady_clock::now() - curTime < .5s && (messageRequests.size() < 2) &&
               mMessageRequestsLocal.try_dequeue(newMessageRequest))
        {
            const auto failedSends = forwardMessageRequest(newMessageRequest, *localSendSockets);
            if (failedSends.any())
            {
                newMessageRequest.destinationNodes = failedSends;
                newMessageRequest.kRequestCreationTime = std::chrono::steady_clock::now();
                messageRequests.push_back(std::move(newMessageRequest));
            }
        }

        for (auto it = std::begin(messageRequests); it != std::end(messageRequests) && not is_test_over();)
        {
            const auto failedSends = forwardMessageRequest(*it, *localSendSockets);
            const bool isSuccess = failedSends.none();
            const bool isRequestStale =
                kMaxMessageRetryTime < std::chrono::steady_clock::now() - it->kRequestCreationTime;
            if (isSuccess)
            {
                it = messageRequests.erase(it);
            }
            else if (isRequestStale)
            {
                SPDLOG_CRITICAL("SEND REQUEST IS STALE, DELETING foreignRSM=true");
                it = messageRequests.erase(it);
            }
            else
            {
                // increment to the next element, return to this later
                it->destinationNodes = failedSends;
                it++;
            }
        }
    }

    SPDLOG_INFO("Pipeline Sending Thread Exiting");

    const auto finishTime = std::chrono::steady_clock::now();
    closeSockets(*localSendSockets, finishTime);
}

void Pipeline::runForeignSendThread(std::unique_ptr<std::vector<nng_socket>> foreignSendSockets)
{
    constexpr auto kMaxMessageRetryTime = 1s;

    bindThreadToCpu(4);

    std::list<pipeline::SendMessageRequest> messageRequests;

    SPDLOG_INFO("Pipeline Sending Thread Starting");
    while (not mShouldThreadStop.load(std::memory_order_relaxed))
    {
        pipeline::SendMessageRequest newMessageRequest;
        const auto curTime = std::chrono::steady_clock::now();

        while (std::chrono::steady_clock::now() - curTime < .5s && (messageRequests.size() < 2) &&
               mMessageRequestsForeign.try_dequeue(newMessageRequest))
        {
            const auto failedSends = forwardMessageRequest(newMessageRequest, *foreignSendSockets);
            if (failedSends.any())
            {
                newMessageRequest.destinationNodes = failedSends;
                newMessageRequest.kRequestCreationTime = std::chrono::steady_clock::now();
                messageRequests.push_back(std::move(newMessageRequest));
            }
        }

        for (auto it = std::begin(messageRequests); it != std::end(messageRequests) && not is_test_over();)
        {
            const auto failedSends = forwardMessageRequest(*it, *foreignSendSockets);
            const bool isSuccess = failedSends.none();
            const bool isRequestStale =
                kMaxMessageRetryTime < std::chrono::steady_clock::now() - it->kRequestCreationTime;
            if (isSuccess)
            {
                it = messageRequests.erase(it);
            }
            else if (isRequestStale)
            {
                SPDLOG_CRITICAL("SEND REQUEST IS STALE, DELETING foreignRSM=false");
                it = messageRequests.erase(it);
            }
            else
            {
                // increment to the next element, return to this later
                it->destinationNodes = failedSends;
                it++;
            }
        }
    }

    SPDLOG_INFO("Pipeline Sending Thread Exiting");

    const auto finishTime = std::chrono::steady_clock::now();
    closeSockets(*foreignSendSockets, finishTime);
}

void Pipeline::runForeignReceiveThread(std::unique_ptr<std::vector<nng_socket>> foreignReceiveSockets)
{
    bindThreadToCpu(5);
    boost::circular_buffer<pipeline::foreign_nng_message> foreignMessages(2048);

    while (not mShouldThreadStop.load(std::memory_order_relaxed))
    {
        for (uint64_t senderId = 0; senderId < foreignReceiveSockets->size(); senderId++)
        {
            const auto originalSize = foreignMessages.size();
            const auto &senderSocket = foreignReceiveSockets->at(senderId);
            std::optional<pipeline::nng_message> message;

            while (not foreignMessages.full() && foreignMessages.size() - originalSize < 256 &&
                   (message = receiveMessage(senderSocket)))
            {
                if (not mForeignReceivedMessageQueue.try_enqueue(pipeline::foreign_nng_message{*message, senderId}))
                {
                    foreignMessages.push_back(pipeline::foreign_nng_message{*message, senderId});
                    break;
                }
            }
            while (not foreignMessages.full() && foreignMessages.size() - originalSize < 256 &&
                   (message = receiveMessage(senderSocket)))
            {
                foreignMessages.push_back(pipeline::foreign_nng_message{*message, senderId});
            }
            while ((not foreignMessages.empty() && mForeignReceivedMessageQueue.try_enqueue(foreignMessages.front())))
            {
                foreignMessages.pop_front();
            }
        }
    }

    for (auto &msg : foreignMessages)
    {
        free_nng_message(msg.message);
    }

    const auto finishTime = std::chrono::steady_clock::now();
    closeSockets(*foreignReceiveSockets, finishTime);
}

void Pipeline::runLocalReceiveThread(std::unique_ptr<std::vector<nng_socket>> localReceiveSockets)
{
    bindThreadToCpu(3);
    boost::circular_buffer<pipeline::nng_message> localMessages(2048);

    localReceiveSockets->erase(localReceiveSockets->begin() + kOwnConfiguration.kNodeId);

    while (not mShouldThreadStop.load(std::memory_order_relaxed))
    {
        for (uint64_t senderId = 0; senderId < localReceiveSockets->size(); senderId++)
        {
            const auto originalSize = localMessages.size();
            const auto &senderSocket = localReceiveSockets->at(senderId);
            std::optional<pipeline::nng_message> message;

            while (not localMessages.full() && localMessages.size() - originalSize < 256 &&
                   (message = receiveMessage(senderSocket)))
            {
                if (not mLocalReceivedMessageQueue.try_enqueue(*message))
                {
                    localMessages.push_back(*message);
                    break;
                }
            }
            while (not localMessages.full() && localMessages.size() - originalSize < 256 &&
                   (message = receiveMessage(senderSocket)))
            {
                localMessages.push_back(*message);
            }
            while ((not localMessages.empty() && mLocalReceivedMessageQueue.try_enqueue(localMessages.front())))
            {
                localMessages.pop_front();
            }
        }
    }

    for (auto &msg : localMessages)
    {
        free_nng_message(msg);
    }

    const auto finishTime = std::chrono::steady_clock::now();
    closeSockets(*localReceiveSockets, finishTime);
}

/* This function is used to send message to a specific node in other RSM.
 *
 * @param nid is the identifier of the node in the other RSM.
 */
void Pipeline::SendToOtherRsm(const uint64_t receivingNodeId, scrooge::CrossChainMessage &message)
{
    constexpr auto kSleepTime = 1ns;

    SPDLOG_DEBUG("Queueing Send message to other RSM: nodeId = {}, message = [SequenceId={}, AckId={}, size='{}']",
                 receivingNodeId, message.data().sequence_number(), getLogAck(message),
                 message.data().message_content().size());

    // MAC signing TODO
    const auto mdata = message.data();
    const auto mstr = mdata.SerializeAsString();
    // Sign with own key.
    std::string encoded = CmacSignString(get_priv_key(), mstr);
    message.set_validity_proof(encoded);
    // std::cout << "Mac: " << encoded << std::endl;

    const auto messageData = message.SerializeAsString();
    pipeline::SendMessageRequest::DestinationSet broadcastSet{};
    broadcastSet.set(receivingNodeId);

    while (not is_test_over() && not mMessageRequestsForeign.try_enqueue(pipeline::SendMessageRequest{
                                     .kRequestCreationTime = std::chrono::steady_clock::now(),
                                     .destinationNodes = broadcastSet,
                                     .messageData = std::move(messageData)}))
    {
        std::this_thread::sleep_for(kSleepTime);
    }
}

/* This function is used to send message to a specific node in other RSM.
 *
 * @param nid is the identifier of the node in the other RSM.
 */
void Pipeline::SendToAllOtherRsm(scrooge::CrossChainMessage &message)
{
    constexpr auto kSleepTime = 1ns;
    const auto foreignNetworkSize = kOwnConfiguration.kOtherNetworkSize;

    const auto messageData = message.SerializeAsString();
    pipeline::SendMessageRequest::DestinationSet broadcastSet{};
    broadcastSet |= -1ULL ^ (-1ULL << foreignNetworkSize);

    while (not is_test_over() && not mMessageRequestsForeign.try_enqueue(pipeline::SendMessageRequest{
                                     .kRequestCreationTime = std::chrono::steady_clock::now(),
                                     .destinationNodes = broadcastSet,
                                     .messageData = std::move(messageData)}))
    {
        std::this_thread::sleep_for(kSleepTime);
    }
}

/* This function will send as many messages from in as possible without blocking
 *
 */
void Pipeline::BroadcastToOwnRsm(boost::circular_buffer<pipeline::ReceivedCrossChainMessage> &in)
{
    const auto ownId = kOwnConfiguration.kNodeId;
    const auto localNetworkSize = kOwnConfiguration.kOwnNetworkSize;
    pipeline::SendMessageRequest::DestinationSet broadcastSet{};
    broadcastSet |= -1ULL ^ (-1ULL << localNetworkSize) ^ (1ULL << ownId);

    while (not is_test_over() && not in.empty())
    {
        auto &message = in.front().message;

        const auto messageData = message.SerializeAsString();
        const auto wasSent = mMessageRequestsLocal.try_enqueue(
            pipeline::SendMessageRequest{.kRequestCreationTime = std::chrono::steady_clock::now(),
                                         .destinationNodes = broadcastSet,
                                         .messageData = std::move(messageData)});
        if (not wasSent)
        {
            return;
        }
        in.pop_front();
    }
}

/* This function is used to receive messages from the other RSM.
 *
 */
void Pipeline::RecvFromOtherRsm(boost::circular_buffer<pipeline::ReceivedCrossChainMessage> &out)
{
    pipeline::foreign_nng_message nng_message;
    while (not is_test_over() && not out.full() && mForeignReceivedMessageQueue.try_dequeue(nng_message))
    {
        auto scroogeMessage = deserializeProtobuf<scrooge::CrossChainMessage>(nng_message.message);

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
            SPDLOG_CRITICAL("NODE {} COULD NOT VERIFY MESSAGE FROM NODE {}", kOwnConfiguration.kNodeId, nng_message.senderId);
        }

        out.push_back(pipeline::ReceivedCrossChainMessage{std::move(scroogeMessage), nng_message.senderId});
    }
}

/* This function is used to receive messages from the nodes in own RSM.
 *
 */
void Pipeline::RecvFromOwnRsm(boost::circular_buffer<scrooge::CrossChainMessage> &out)
{
    pipeline::nng_message nng_message;
    while (not is_test_over() && not out.full() && mLocalReceivedMessageQueue.try_dequeue(nng_message))
    {
        auto scroogeMessage = deserializeProtobuf<scrooge::CrossChainMessage>(nng_message);
        out.push_back(std::move(scroogeMessage));
    }
}

/* This function will send keys to own RSM.
 */
void Pipeline::BroadcastKeyToOwnRsm()
{
    const auto ownId = kOwnConfiguration.kNodeId;
    const auto localNetworkSize = kOwnConfiguration.kOwnNetworkSize;
    pipeline::SendMessageRequest::DestinationSet broadcastSet{};
    broadcastSet |= -1ULL ^ (-1ULL << localNetworkSize) ^ (1ULL << ownId);

    scrooge::KeyExchangeMessage msg;
    msg.clear_node_key();
    msg.clear_node_id();
    std::cout << "Actual: " << get_priv_key() << std::endl;
    msg.set_node_key(get_priv_key());
    msg.set_node_id(ownId);

    const auto messageData = msg.SerializeAsString();
    const auto wasSent = mMessageRequestsLocal.try_enqueue(
        pipeline::SendMessageRequest{.kRequestCreationTime = std::chrono::steady_clock::now(),
                                     .destinationNodes = broadcastSet,
                                     .messageData = std::move(messageData)});

    std::cout << "Sent? " << wasSent << std::endl;
}

/* This function will send keys to other RSM.
 */
void Pipeline::BroadcastKeyToOtherRsm()
{
    const auto ownId = kOwnConfiguration.kNodeId;
    const auto foreignNetworkSize = kOwnConfiguration.kOtherNetworkSize;
    pipeline::SendMessageRequest::DestinationSet broadcastSet{};
    broadcastSet |= -1ULL ^ (-1ULL << foreignNetworkSize);

    scrooge::KeyExchangeMessage msg;
    msg.set_node_key(get_priv_key());
    msg.set_node_id(ownId);

    const auto messageData = msg.SerializeAsString();

    mMessageRequestsForeign.wait_enqueue(
        pipeline::SendMessageRequest{.kRequestCreationTime = std::chrono::steady_clock::now(),
                                     .destinationNodes = broadcastSet,
                                     .messageData = std::move(messageData)});
}

/* This function is used to receive messages from the nodes in own RSM.
 */
void Pipeline::RecvFromOwnRsm()
{
    const auto ownId = kOwnConfiguration.kNodeId;
    const auto localNetworkSize = kOwnConfiguration.kOwnNetworkSize;

    uint64_t count = 0;
    pipeline::nng_message nng_message;
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
}

/* This function is used to receive messages from the other RSM.
 */
void Pipeline::RecvFromOtherRsm()
{
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
}
