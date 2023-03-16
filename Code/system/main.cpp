#include "benchmark.h"
#include "global.h"
#include "parser.h"
#include "nng/nng.h"
#include <nng/protocol/pipeline0/pull.h>
#include <nng/protocol/pipeline0/push.h>
#include <functional>
#include <random>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>
#include <stdio.h>
#include <fstream>
#include <pthread.h>
#include <sched.h>

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

    return socket;
}

/* Sending data to nodes of other RSM.
 *
 * @param buf is the outgoing message of protobuf type.
 * @param socket is the nng_socket to put the data into
 */
static int sendMessage(const nng_socket &socket, const std::string& data, bool useNonblocking)
{
    const auto bufferSize = data.size();
    const auto useNonBlock = NNG_FLAG_NONBLOCK * useNonblocking;
    const auto sendReturnValue = nng_send(socket, const_cast<char *>(data.c_str()), bufferSize, useNonBlock);
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
static bool receiveMessage(const nng_socket &socket, bool useNonblocking)
{
    char *buffer;
    size_t bufferSize;
    const auto useNonBlock = NNG_FLAG_NONBLOCK * useNonblocking;

    const auto receiveReturnValue = nng_recv(socket, &buffer, &bufferSize, NNG_FLAG_ALLOC | useNonBlock);

    if (receiveReturnValue != 0)
    {
        if (receiveReturnValue != nng_errno_enum::NNG_EAGAIN)
        {
            SPDLOG_CRITICAL("nng_recv has error value = {}", nng_strerror(receiveReturnValue));
        }

        return false;
    }

    //std::string receivedData{buffer, bufferSize};
    DoNotOptimize(buffer[0]);
    DoNotOptimize(buffer[bufferSize/2]);
    DoNotOptimize(buffer[bufferSize-1]);
    assert(bufferSize == get_packet_size());

    nng_free(buffer, bufferSize);

    return true;
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
    SPDLOG_CRITICAL("PACKET SIZE={}", get_packet_size());

    const auto kOwnNetworkUrls = parseNetworkUrls(get_rsm_id() ? kNetworkOneConfigPath : kNetworkZeroConfigPath);
    const auto kOtherNetworkUrls = parseNetworkUrls(get_other_rsm_id() ? kNetworkOneConfigPath : kNetworkZeroConfigPath);

    const auto &kOwnUrl = kOwnNetworkUrls.at(kOwnConfiguration.kNodeId);

    for (size_t localNodeId = 0; localNodeId < kOwnNetworkUrls.size(); localNodeId++)
    {
        if (kOwnConfiguration.kNodeId == localNodeId)
        {
            //mLocalSendSockets.emplace_back();
            //mLocalReceiveSockets.emplace_back();
            continue;
        }

        const auto &localUrl = kOwnNetworkUrls.at(localNodeId);
        const auto sendingPort = getSendPort(localNodeId, false);
        const auto receivingPort = getReceivePort(localNodeId, false);

        auto sendingUrl = "tcp://" + localUrl + ":" + std::to_string(sendingPort);
        auto receivingUrl = "tcp://" + kOwnUrl + ":" + std::to_string(receivingPort);

        mLocalSendSockets.emplace_back(openSendSocket(sendingUrl));
        mLocalReceiveSockets.emplace_back(openReceiveSocket(receivingUrl));
    }

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

    const auto testDuration = 30s;
    const auto warmUpTime = 10s;

    std::string data(get_packet_size(), ' ');
    assert(data.length() == get_packet_size());

    std::mt19937 gen(12345678);
    std::uniform_int_distribution<> distrib(1, 255);
    for (auto& c : data)
    {
        c = distrib(gen);
    }

    const auto startTime = std::chrono::steady_clock::now();

    const auto sendNonBlocking = [&]() -> bool{
        const auto elapsedTime = std::chrono::steady_clock::now() - startTime;
        return 1;
    };

    const auto recvNonBlocking = [&]() -> bool{
        const auto elapsedTime = std::chrono::steady_clock::now() - startTime;
        return 1;
    };
    
    auto foreignSendThread0 = std::thread([&](){
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(0, &cpuset);
        int rc = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
        if (rc != 0) {
            SPDLOG_CRITICAL("SET AFFINITY FAILED ERR = {}", rc);
        }

        while (std::chrono::steady_clock::now() - startTime < testDuration + 1s)
        {
            sendMessage(mForeignSendSockets[0], data, sendNonBlocking());
        }
    });

    auto foreignSendThread1 = std::thread([&](){
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(2, &cpuset);
        int rc = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
        if (rc != 0) {
            SPDLOG_CRITICAL("SET AFFINITY FAILED ERR = {}", rc);
        }

        while (std::chrono::steady_clock::now() - startTime < testDuration + 1s)
        {
            sendMessage(mForeignSendSockets[1], data, sendNonBlocking());
        }
    });

    auto localSendThread = std::thread([&](){
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(4, &cpuset);
        int rc = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
        if (rc != 0) {
            SPDLOG_CRITICAL("SET AFFINITY FAILED ERR = {}", rc);
        }

        while (std::chrono::steady_clock::now() - startTime < testDuration + 1s)
        {
            for (auto& socket : mLocalSendSockets)
            {
                sendMessage(socket, data, sendNonBlocking());
            }
        }
    });

    std::atomic<uint64_t> globalNumMessages{};

    auto foreignReceiveThread0 = std::thread([&](){
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(6, &cpuset);
        int rc = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
        if (rc != 0) {
            SPDLOG_CRITICAL("SET AFFINITY FAILED ERR = {}", rc);
        }

        uint64_t numMessages{};
        bool shouldCountMessage{};
        while (std::chrono::steady_clock::now() - startTime < testDuration + 1s)
        {
            const auto elapsedTime = std::chrono::steady_clock::now() - startTime;
            shouldCountMessage = warmUpTime < elapsedTime && elapsedTime < testDuration;
            numMessages += shouldCountMessage & receiveMessage(mForeignReceiveSockets[0], recvNonBlocking());
        }
        globalNumMessages += numMessages;
    });

    auto foreignReceiveThread1 = std::thread([&](){
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(8, &cpuset);
        int rc = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
        if (rc != 0) {
            SPDLOG_CRITICAL("SET AFFINITY FAILED ERR = {}", rc);
        }

        uint64_t numMessages{};
        bool shouldCountMessage{};
        while (std::chrono::steady_clock::now() - startTime < testDuration + 1s)
        {
            const auto elapsedTime = std::chrono::steady_clock::now() - startTime;
            shouldCountMessage = warmUpTime < elapsedTime && elapsedTime < testDuration;
            numMessages += shouldCountMessage & receiveMessage(mForeignReceiveSockets[1], recvNonBlocking());
        }
        globalNumMessages += numMessages;
    });

    auto localReceiveThread = std::thread([&](){
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(10, &cpuset);
        int rc = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
        if (rc != 0) {
            SPDLOG_CRITICAL("SET AFFINITY FAILED ERR = {}", rc);
        }

        uint64_t numMessages{};
        bool shouldCountMessage{};
        while (std::chrono::steady_clock::now() - startTime < testDuration + 1s)
        {
            const auto elapsedTime = std::chrono::steady_clock::now() - startTime;
            shouldCountMessage = warmUpTime < elapsedTime && elapsedTime < testDuration;
            for (auto& socket : mLocalReceiveSockets)
            {
                numMessages += shouldCountMessage & receiveMessage(socket, recvNonBlocking());
            }
        }
        globalNumMessages += numMessages;
    });

    localReceiveThread.join();
    localSendThread.join();
    foreignSendThread0.join();
    foreignReceiveThread0.join();
    foreignSendThread1.join();
    foreignReceiveThread1.join();

    remove(kOwnConfiguration.kLogPath.c_str());
    std::ofstream file{kOwnConfiguration.kLogPath, std::ios_base::binary};

    if (!file.is_open())
    {
        SPDLOG_CRITICAL("COULD NOT SAVE TO FILE");
    }

    file << "message_size: " << get_packet_size() << '\n';
    file << "messages_received: " << globalNumMessages << '\n';
    file << "duration_seconds: " << std::chrono::duration<double>{testDuration - warmUpTime}.count() << '\n';
    // file << "average_latency: " << averageLatency << '\n';
    file << "transfer_strategy: " << "3Send3Recv Threads NonBlocking SendRecv" << '\n';
    file.close();

    return 0;
}
