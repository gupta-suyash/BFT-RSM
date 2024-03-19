#include "acknowledgment.h"
#include "all_to_all.h"
#include "config.h"
#include "global.h"
#include "iothread.h"
#include "one_to_one.h"
#include "parser.h"
#include "pipeline.h"
#include "quorum_acknowledgment.h"
#include "scrooge.h"
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include "ipc.h"
#include <fstream>
#include <cstring>
#include <filesystem>
#include <sys/errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <thread>
#include "scrooge_message.pb.h"
#include "scrooge_request.pb.h"

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        SPDLOG_CRITICAL("Incorrect usage, call as `./scrooge pipe_path`");
        return 1;
    }

    constexpr auto kMessageSizeBytes = 100;
    const auto pipeWritingSpeed = 1.s / 10'000'000;
    const auto pipePath = std::string(argv[1]);

    createPipe(pipePath);

    std::ofstream pipe{pipePath, std::ios_base::app};
    if (!pipe.is_open())
    {
        SPDLOG_CRITICAL("Open Failed={}, {}", std::strerror(errno), getlogin());
        return 1;
    }
    else
    {
        SPDLOG_CRITICAL("Open Success");
    }

    scrooge::ScroogeRequest scroogeRequest; 
    auto messageContent = scroogeRequest.mutable_send_message_request()->mutable_content();
    messageContent->set_message_content(std::string(kMessageSizeBytes, 'L'));

    uint64_t curSequenceNumber{};
    while (pipe.is_open())
    {
        const auto curTime = std::chrono::steady_clock::now();
       
        messageContent->set_sequence_number(curSequenceNumber);
        const auto serializedRequest = scroogeRequest.SerializeAsString();
        writeMessage(pipe, serializedRequest);

        curSequenceNumber++;
        std::this_thread::sleep_until(curTime + pipeWritingSpeed);
    }

    SPDLOG_CRITICAL("Pipe is closed, exiting...");

    return 0;
}
