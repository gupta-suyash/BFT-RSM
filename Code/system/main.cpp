#include "benchmark.h"
#include "global.h"
#include "parser.h"
#include "nng/nng.h"
#include <nng/protocol/pipeline0/pull.h>
#include <nng/protocol/pipeline0/push.h>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>

static constexpr uint64_t kMinimumPortNumber = 7000;
static NodeConfiguration kOwnConfiguration;
std::vector<nng_socket> mLocalReceiveSockets;
std::vector<nng_socket> mForeignReceiveSockets;
std::vector<nng_socket> mLocalSendSockets;
std::vector<nng_socket> mForeignSendSockets;


static nng_socket openReceiveSocket(const std::string &url)
{
    constexpr auto kResultOpenSuccessful = 0;

    nng_socket socket;
    const auto openResult = nng_pull0_open(&socket);
    const bool cannotOpenSocket = kResultOpenSuccessful != openResult;
    if (cannotOpenSocket)
    {
        SPDLOG_CRITICAL("Cannot open socket for receiving on URL {} ERROR: {}", url, nng_strerror(openResult));
        exit(1);
    }

    // Asynchronous wait for someone to listen to this socket.
    const auto nngDialResult = nng_listen(socket, url.c_str(), nullptr, 0);

    if (nngDialResult != 0)
    {
        SPDLOG_CRITICAL("CANNOT OPEN SOCKET FOR LISTENING. URL = '{}' Return value {}", url,
                        nng_strerror(nngDialResult));
        exit(1);
    }

    SPDLOG_INFO("NNG listen for URL = '{}' has return value {} ", url, nng_strerror(nngDialResult));

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
        exit(1);
    }

    // Asynchronous wait for someone to listen to this socket.
    const auto nngDialResult = nng_dial(socket, url.c_str(), nullptr, NNG_FLAG_NONBLOCK);
    SPDLOG_INFO("NNG dial for URL = '{}' has return value {} ", url, nng_strerror(nngDialResult));

    return socket;
}

/* Sending data to nodes of other RSM.
 *
 * @param buf is the outgoing message of protobuf type.
 * @param socket is the nng_socket to put the data into
 */
static int sendMessage(const nng_socket &socket, const std::string& data)
{
    const auto bufferSize = data.size();
    const auto sendReturnValue = nng_send(socket, const_cast<char *>(data.c_str()), bufferSize, NNG_FLAG_NONBLOCK);
    const bool isActualError = sendReturnValue != 0 && sendReturnValue != nng_errno_enum::NNG_EAGAIN;
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
static std::optional<std::string> receiveMessage(const nng_socket &socket)
{
    char *buffer;
    size_t bufferSize;

    const auto receiveReturnValue = nng_recv(socket, &buffer, &bufferSize, NNG_FLAG_ALLOC | NNG_FLAG_NONBLOCK);

    if (receiveReturnValue != 0)
    {
        if (receiveReturnValue != nng_errno_enum::NNG_EAGAIN)
        {
            SPDLOG_CRITICAL("nng_recv has error value = {}", nng_strerror(receiveReturnValue));
        }

        return std::nullopt;
    }

    std::string receivedData{buffer, bufferSize};

    nng_free(buffer, bufferSize);

    return receivedData;
}

/* Returns the port the current node will use to receive from senderId
 * Current port strategy is that all nodes will listen to local traffic from node i on port `kMinimumPortNumber + i`
 * All nodes will also listen to foreign traffic from node j on port `kMinimumPortNumber + kSizeOfOwnNetwork + j`
 */
static uint64_t getReceivePort(uint64_t senderId, bool isForeign)
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
uint64_t getSendPort(uint64_t receiverId, bool isForeign)
{
    if (isForeign)
    {
        return kMinimumPortNumber + kOwnConfiguration.kOtherNetworkSize + kOwnConfiguration.kNodeId;
    }
    return kMinimumPortNumber + kOwnConfiguration.kNodeId;
}

int main(int argc, char *argv[])
{
    kOwnConfiguration = parser(argc, argv);
        const auto &[kOwnNetworkSize, kOtherNetworkSize, kOwnMaxNumFailedNodes, kOtherMaxNumFailedNodes, kNodeId, kLogPath,
                 kWorkingDir] = kOwnConfiguration;
    const auto kNetworkZeroConfigPath = kWorkingDir + "network0urls.txt"s;
    const auto kNetworkOneConfigPath = kWorkingDir + "network1urls.txt"s;
    SPDLOG_INFO("Config set: kNumLocalNodes = {}, kNumForeignNodes = {}, kMaxNumLocalFailedNodes = {}, "
                "kMaxNumForeignFailedNodes = {}, kOwnNodeId = {}, g_rsm_id = {}, num_packets = {},  kLogPath= '{}'",
                kOwnNetworkSize, kOtherNetworkSize, kOwnMaxNumFailedNodes, kOtherMaxNumFailedNodes, kNodeId,
                get_rsm_id(), get_number_of_packets(), kLogPath);

    const auto kOwnNetworkUrls = parseNetworkUrls(get_rsm_id() ? kNetworkOneConfigPath : kNetworkZeroConfigPath);
    const auto kOtherNetworkUrls = parseNetworkUrls(get_other_rsm_id() ? kNetworkOneConfigPath : kNetworkZeroConfigPath);

    const auto &kOwnUrl = kOwnNetworkUrls.at(kOwnConfiguration.kNodeId);

    SPDLOG_INFO("Configuring local sockets: {}", kOwnUrl);
    for (size_t localNodeId = 0; localNodeId < kOwnNetworkUrls.size(); localNodeId++)
    {
        // if (kOwnConfiguration.kNodeId == localNodeId)
        // {
        //     mLocalSendSockets.emplace_back();
        //     mLocalReceiveSockets.emplace_back();
        //     continue;
        // }

        const auto &localUrl = kOwnNetworkUrls.at(localNodeId);
        const auto sendingPort = getSendPort(localNodeId, false);
        const auto receivingPort = getReceivePort(localNodeId, false);

        auto sendingUrl = "tcp://" + localUrl + ":" + std::to_string(sendingPort);
        auto receivingUrl = "tcp://" + kOwnUrl + ":" + std::to_string(receivingPort);

        mLocalSendSockets.emplace_back(openSendSocket(sendingUrl));
        mLocalReceiveSockets.emplace_back(openReceiveSocket(receivingUrl));
    }

    SPDLOG_INFO("Configuring foreign sockets");
    for (size_t foreignNodeId = 0; foreignNodeId < kOtherNetworkUrls.size(); foreignNodeId++)
    {
        const auto &foreignUrl = kOtherNetworkUrls.at(foreignNodeId);
        const auto sendingPort = getSendPort(foreignNodeId, true);
        const auto receivingPort = getReceivePort(foreignNodeId, true);

        auto sendingUrl = "tcp://" + foreignUrl + ":" + std::to_string(sendingPort);
        auto receivingUrl = "tcp://" + kOwnUrl + ":" + std::to_string(receivingPort);

        mForeignSendSockets.emplace_back(openSendSocket(sendingUrl));
        mForeignReceiveSockets.emplace_back(openReceiveSocket(receivingUrl));
    }

    const auto kSize = get_packet_size();

    auto sendThread = std::thread([&](){
        std::string data(' ', kSize);
        while (true)
        {
            for (auto& socket : mLocalSendSockets)
            {
                sendMessage(socket, data.c_str());
            }
            for (auto& socket : mForeignSendSockets)
            {
                sendMessage(socket, data.c_str());
            }
        }
    });

    auto receiveThread = std::thread([&](){
        while (true)
        {
            for (auto& socket : mLocalReceiveSockets)
            {
                auto x = receiveMessage(socket);
                DoNotOptimize(x);
            }
            for (auto& socket : mForeignReceiveSockets)
            {
                auto x = receiveMessage(socket);
                DoNotOptimize(x);
            }
        }
    });

    receiveThread.join();
    sendThread.join();

    return 0;
}
