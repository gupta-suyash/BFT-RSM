#include <filesystem>
#include <memory>
#include <pwd.h>
#include <string>

#include "acknowledgement.h"
#include "connect.h"
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

    unique_ptr<Pipeline> pipe_obj = make_unique<Pipeline>();
    pipe_ptr = pipe_obj.get();
    pipe_ptr->SetSockets();
    SPDLOG_INFO("Done setting up sockets between nodes.");

    // Setting up the queue.
	double wait_time = 5;
    unique_ptr<PipeQueue> sp_queue = make_unique<PipeQueue>(wait_time);
    sp_qptr = sp_queue.get();
    SPDLOG_INFO("Done setting up msg-queue and store-queue between threads.");

    // The next command is for testing the queue.
    // sp_qptr->CallThreads();

    SPDLOG_INFO("Done setting up the in-queue for messages from protocol.");

    // Creating and starting Sender IOThreads.
    unique_ptr<SendThread> snd_obj = make_unique<SendThread>();
    snd_obj->Init(0);
    SPDLOG_INFO("Created Sender Thread with ID={} ", snd_obj->GetThreadId());

    // Creating and starting Receiver IOThreads.
    // unique_ptr<RecvThread> rcv_obj = make_unique<RecvThread>();
    // rcv_obj->Init(1);
    // cout << "Created Receiver Thread: " << rcv_obj->GetThreadId() << endl;

    snd_obj->thd_.join();
    // rcv_obj->thd_.join();

    return (1);
}
