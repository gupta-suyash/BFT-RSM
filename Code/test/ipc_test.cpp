#include <boost/test/unit_test.hpp>
#include <thread>

#include "ipc.h"

namespace ipc_test
{

BOOST_AUTO_TEST_CASE(test_ipc)
{
    const auto pipe = "/tmp/ipctest";
    boost::lockfree::spsc_queue<std::vector<uint8_t>> readMessages(1);
    boost::lockfree::spsc_queue<std::vector<uint8_t>> writeMessages(1);
    std::atomic_bool exitReader{}, exitWriter{};

    createPipe(pipe);

    auto reader = std::thread(startPipeReader, pipe, &readMessages, std::cref(exitReader));
    auto writer = std::thread(startPipeWriter, pipe, &writeMessages, std::cref(exitWriter));

    for (auto test = 0; test < (1 << 8); test++)
    {
        const auto message = std::vector<uint8_t>(1'000'000, test);

        while (!writeMessages.push(std::move(message)))
        {
            std::this_thread::sleep_for(std::chrono::milliseconds{10});
        }

        std::vector<uint8_t> readMessage;
        while (!readMessages.pop(readMessage))
        {
            std::this_thread::sleep_for(std::chrono::milliseconds{10});
        }

        BOOST_CHECK(message == readMessage);
    }
    // handle annoying deadlock because made in haste
    exitReader = true;
    while (!writeMessages.push(std::vector<uint8_t>(3)))
    {
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
    }
    exitWriter = true;

    reader.join();
    writer.join();
}

}; // namespace ipc_test
