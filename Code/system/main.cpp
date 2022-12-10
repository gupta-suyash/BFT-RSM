#include <filesystem>
#include <memory>
#include <pwd.h>
#include <string>
#include <thread>

#include "acknowledgement.h"
#include "global.h"
#include "iothread.h"
#include "pipe_queue.h"
#include "pipeline.h"
#include "quorum_acknowledgement.h"

using std::filesystem::current_path;

void parser(int argc, char *argv[]);

int main(int argc, char *argv[])
{
    // Parsing the command line args.
    parser(argc, argv);
    // Setting up the Acknowledgment object.
    ack_obj = new Acknowledgment();

    constexpr uint64_t kQuorumSize = 1;
    QuorumAcknowledgment quack_obj(kQuorumSize);

    const auto pipeline = std::make_shared<Pipeline>();
    const auto pipeQueue = std::make_shared<PipeQueue>(5s);
    sp_qptr = pipeQueue.get(); // remove?
    pipeline->SetSockets();
    SPDLOG_INFO("Done setting up sockets between nodes.");

    // Setting up the queue.
    SPDLOG_INFO("Done setting up msg-queue and store-queue between threads.");

    // The next command is for testing the queue.
    // sp_qptr->CallThreads();

    SPDLOG_INFO("Done setting up the in-queue for messages from protocol.");

    auto sendThread = std::thread(runSendThread, pipeQueue, pipeline);
    SPDLOG_INFO("Created Sender Thread with ID={} ", sendThread.get_id());

    auto receiveThread = std::thread(runReceiveThread, pipeline);
    SPDLOG_INFO("Created Receiver Thread with ID={} ", receiveThread.get_id());

    sendThread.join();
    receiveThread.join();

    return 0;
}
