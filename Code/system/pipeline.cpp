#include "pipeline.h"
#include "acknowledgment.h"

#include <chrono>
#include <list>

int64_t getLogAck(const scrooge::CrossChainMessage &message)
{
    if (!message.has_ack_count())
    {
        return -1;
    }
    return message.ack_count().value();
}

nng_socket makeDialedSocket(const std::string& url)
{
    constexpr auto resultOpenSuccessful = 0;
    
    nng_socket socket;
    const auto openResult = nng_push0_open(&socket);
    if (resultOpenSuccessful != openResult)
    {
        SPDLOG_CRITICAL("Cannot open socket for reading URL {} ERROR: {}", url, nng_strerror(openResult));
        exit(1);
    }

    // Asynchronous wait for someone to listen to this socket.
    const auto nngDialResult = nng_dial(socket, url.c_str(), nullptr, NNG_FLAG_NONBLOCK);
    SPDLOG_INFO("NNG dial for URL = '{}' has return value {} ", url, nngDialResult);

    return socket;
}

/* Sending data to nodes of other RSM.
 *
 * @param buf is the outgoing message of protobuf type.
 * @param socket is the nng_socket to put the data into
 */
int sendMessage(const nng_socket& socket, const scrooge::CrossChainMessage &buf)
{
    string buffer;
    buf.SerializeToString(&buffer);

    const auto bufferSize = buffer.size();
    const auto sendReturnValue = nng_send(socket, const_cast<char*>(buffer.c_str()), bufferSize, NNG_FLAG_NONBLOCK);

    const bool isActualError = sendReturnValue != 0 && sendReturnValue != nng_errno_enum::NNG_EAGAIN;
    if (isActualError)
    {
        SPDLOG_CRITICAL("nng_recv has error value = {}", nng_strerror(sendReturnValue));
    }

    return sendReturnValue;
}

/* Receving data from nodes of other RSM.
 *
 * @param socket is the nng_socket to check for data on.
 * @return the protobuf if data for one was contained in the socket.
 */
std::optional<scrooge::CrossChainMessage> receiveMessage(const nng_socket& socket)
{
    char *buffer;
    size_t bufferSize;

    // We want the nng_recv to be non-blocking and reduce copies.
    // So, we use the two available bit masks.
    const auto receiveReturnValue = nng_recv(socket, &buffer, &bufferSize, NNG_FLAG_ALLOC | NNG_FLAG_NONBLOCK);

    // nng_recv is non-blocking, if there is no data, return value is non-zero.
    if (receiveReturnValue != 0)
    {
        if (receiveReturnValue != nng_errno_enum::NNG_EAGAIN)
        {
            // Silence error EAGAIN while using nonblocking nng functions
            SPDLOG_CRITICAL("nng_recv has error value = {}", nng_strerror(receiveReturnValue));
        }

        return std::nullopt;
    }

    scrooge::CrossChainMessage receivedMsg;
    receivedMsg.ParseFromArray(buffer, bufferSize);

    nng_free(buffer, bufferSize);

    return receivedMsg;
}

Pipeline::Pipeline(std::vector<std::string>&& ownNetworkUrls, std::vector<std::string>&& otherNetworkUrls, NodeConfiguration ownConfiguration) : kOwnConfiguration(ownConfiguration), kOwnNetworkUrls(std::move(ownNetworkUrls)), kOtherNetworkUrls(std::move(otherNetworkUrls))
{
}

Pipeline::~Pipeline()
{
    const std::scoped_lock lock{mMutex};
    if (not mIsThreadRunning)
    {
        mShouldThreadStop = true;
        messageSendThread.join();
        mIsThreadRunning = false;
    }
}

/* Returns the port the current node will use to receive from senderId
 * Current port strategy is that all nodes will listen to local traffic from node i on port `kMinimumPortNumber + i`
 * All nodes will also listen to foreign traffic from node j on port `kMinimumPortNumber + kSizeOfOwnNetwork + j`
 * Sending ports are the same as listening ports except shifted up by `kSizeOfOwnNetwork + kSizeOfOtherNetwork`
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
 * Sending ports are the same as listening ports except shifted up by `kSizeOfOwnNetwork + kSizeOfOtherNetwork`
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
    const std::scoped_lock lock{mMutex};
    if (mIsThreadRunning)
    {
        return;
    }

    const auto& kOwnUrl = kOwnNetworkUrls.at(kOwnConfiguration.kNodeId);
    auto foreignSendSockets = std::make_unique<std::vector<nng_socket>>();
    auto localSendSockets = std::make_unique<std::vector<nng_socket>>();

    for (const auto& localUrl : kOwnNetworkUrls)
    {
        const auto nodeId = mLocalReceiveSockets.size();
        const auto sendingPort = getSendPort(nodeId, false);
        const auto receivingPort = getSendPort(nodeId, false);

        auto sendingUrl = "tcp://" + kOwnUrl + std::to_string(sendingPort);
        auto receivingUrl = "tcp://" + localUrl + std::to_string(receivingPort);

        localSendSockets->emplace_back(std::move(makeDialedSocket(sendingUrl)));
        mLocalReceiveSockets.emplace_back(std::move(makeDialedSocket(receivingUrl)));
    }

    for (const auto& foreignUrl : kOtherNetworkUrls)
    {
        const auto nodeId = mForeignReceiveSockets.size();
        const auto sendingPort = getSendPort(nodeId, false);
        const auto receivingPort = getSendPort(nodeId, false);

        auto sendingUrl = "tcp://" + kOwnUrl + std::to_string(sendingPort);
        auto receivingUrl = "tcp://" + foreignUrl + std::to_string(receivingPort);

        foreignSendSockets->emplace_back(std::move(sendingUrl));
        mForeignReceiveSockets.emplace_back(std::move(receivingUrl));
    }

    messageSendThread = std::thread(&Pipeline::runSendThread, this, std::move(foreignSendSockets), std::move(localSendSockets));
    mIsThreadRunning = true;
}

void Pipeline::runSendThread(std::unique_ptr<std::vector<nng_socket>> foreignSendSockets, std::unique_ptr<std::vector<nng_socket>> localSendSockets)
{
    constexpr auto kPollPeriod = 1us;
    constexpr auto kMaxMessageRetryTime = 10s;

    std::list<pipeline::SendMessageRequest> messageRequests;

    SPDLOG_INFO("Pipeline Sending Thread Starting");

    while (not mShouldThreadStop)
    {
        const auto curTime = std::chrono::steady_clock::now();

        pipeline::SendMessageRequest newMessageRequest;
        while (mMessageRequests.pop(newMessageRequest))
        {
            messageRequests.emplace_back(std::move(newMessageRequest));
        }

        for (auto it = std::begin(messageRequests); it != std::end(messageRequests);)
        {
            const auto& [kRequestCreationTime, destinationNodeId, isDestinationForeign, message] = *it;

            const bool isRequestStale = kMaxMessageRetryTime < curTime - kRequestCreationTime;
            if (isRequestStale)
            {
                // remove the current element and increment it
                it = messageRequests.erase(it);
                continue;
            }

            const auto socket = [&](){
                if (it->isDestinationForeign)
                {
                    return foreignSendSockets->at(it->destinationNodeId);
                }
                return localSendSockets->at(it->destinationNodeId);
            }();
            
            // send the data
            const auto sendMessageResult = sendMessage(socket, message);

            const bool isSendSuccessful = sendMessageResult == 0;
            if (isSendSuccessful)
            {
                // remove the current element and increment it
                it = messageRequests.erase(it);
                continue;
            }

            const auto isRealError = ! isSendSuccessful && sendMessageResult != nng_errno_enum::NNG_EAGAIN;
            if (isRealError)
            {
                SPDLOG_CRITICAL("NNG_SEND error when sending from node {} to {} Error: {}", kOwnConfiguration.kNodeId, destinationNodeId, nng_strerror(sendMessageResult));
            }

            // increment to the next element, return to this later
            it++;
        }

        std::this_thread::sleep_for(kPollPeriod);
    }
    SPDLOG_INFO("Pipeline Sending Thread Exiting");
}

/* This function is used to send message to a specific node in other RSM.
 *
 * @param nid is the identifier of the node in the other RSM.
 */
void Pipeline::SendToOtherRsm(const uint64_t receivingNodeId, scrooge::CrossChainMessage&& message)
{
    constexpr auto kSleepTime = 1us;

    SPDLOG_DEBUG("Sending message to other RSM: nodeId = {}, message = [SequenceId={}, AckId={}, size='{}']", receivingNodeId,
                 message.data().sequence_number(), getLogAck(message), message.data().message_content().size());

    const auto sendMessageRequest = pipeline::SendMessageRequest{
        .kRequestCreationTime = std::chrono::steady_clock::now(),
        .destinationNodeId = receivingNodeId,
        .isDestinationForeign = true,
        .message = std::move(message)
    };

    while (not mMessageRequests.push(sendMessageRequest))
    {
        std::this_thread::sleep_for(kSleepTime);
    }
}

/* This function is used to receive messages from the other RSM.
 *
 */
std::vector<pipeline::ReceivedCrossChainMessage> Pipeline::RecvFromOtherRsm()
{
    std::vector<pipeline::ReceivedCrossChainMessage> newMessages{};

    for (uint64_t senderId = 0; senderId < mForeignReceiveSockets.size(); senderId++)
    {
        const auto& senderSocket = mForeignReceiveSockets.at(senderId);

        const auto message = receiveMessage(senderSocket);
        if (message.has_value())
        {
            SPDLOG_DEBUG("Received message from other RSM: nodeId = {}, message = [SequenceId={}, AckId={}, size='{}']",
                         senderId, message->data().sequence_number(), getLogAck(*message),
                         message->data().message_content().size());

            // This message needs to broadcasted to other nodes
            // in the RSM, so enqueue in the queue for sender.
            newMessages.emplace_back(
                pipeline::ReceivedCrossChainMessage{.message = std::move(*message), .senderId = senderId});
        }
    }
    return newMessages;
}

/* This function is used to send messages to the nodes in own RSM.
 *
 */
void Pipeline::BroadcastToOwnRsm(scrooge::CrossChainMessage &&message)
{
    constexpr auto kSleepTime = 1us;

    for (uint64_t receiverId = 0; receiverId < kOwnConfiguration.kOwnNetworkSize; receiverId++)
    {
        if (receiverId == kOwnConfiguration.kNodeId)
            continue;

        const auto sendMessageRequest = pipeline::SendMessageRequest{
            .kRequestCreationTime = std::chrono::steady_clock::now(),
            .destinationNodeId = receiverId,
            .isDestinationForeign = false,
            .message = std::move(message)
        };

        while (not mMessageRequests.push(sendMessageRequest))
        {
            std::this_thread::sleep_for(kSleepTime);
        }

        SPDLOG_DEBUG("Sent message to own RSM: nodeId = {}, message = [SequenceId={}, AckId={}, size='{}']", receiverId,
                     message.data().sequence_number(), getLogAck(message), message.data().message_content().size());
    }
}

/* This function is used to receive messages from the nodes in own RSM.
 *
 */
vector<scrooge::CrossChainMessage> Pipeline::RecvFromOwnRsm()
{
    std::vector<scrooge::CrossChainMessage> messages{};

    for (uint64_t senderId = 0; senderId < mLocalReceiveSockets.size(); senderId++)
    {
        const auto& senderSocket = mLocalReceiveSockets.at(senderId);

        const auto message = receiveMessage(senderSocket);
        if (message.has_value())
        {
            SPDLOG_DEBUG("Received message from own RSM: nodeId = {}, message = [SequenceId={}, AckId={}, size='{}']",
                         senderId, message->data().sequence_number(), getLogAck(*message),
                         message->data().message_content().size());

            messages.emplace_back(std::move(*message));
        }
    }

    return messages;
}
