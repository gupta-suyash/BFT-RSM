#include <boost/test/unit_test.hpp>
#include <thread>

#include "ipc.h"

namespace ipc_test
{

BOOST_AUTO_TEST_SUITE(ipc_test)

BOOST_AUTO_TEST_CASE(test_ipc)
{
    const auto pipe = "/tmp/ipctest";
    const auto readMessages = std::make_shared<ipc::DataChannel>(1);
    const auto writeMessages = std::make_shared<ipc::DataChannel>(1);
    const auto exitReader = std::make_shared<std::atomic_bool>();
    const auto exitWriter = std::make_shared<std::atomic_bool>();

    createPipe(pipe);

    auto reader = std::thread(startPipeReader, pipe, readMessages, exitReader);
    auto writer = std::thread(startPipeWriter, pipe, writeMessages, exitWriter);

    for (auto test = 0; test < (1 << 8); test++)
    {
        const auto message = std::vector<uint8_t>(1'000'000, test);

        while (!writeMessages->push(std::move(message)))
        {
            std::this_thread::sleep_for(std::chrono::milliseconds{10});
        }

        std::vector<uint8_t> readMessage;
        while (!readMessages->pop(readMessage))
        {
            std::this_thread::sleep_for(std::chrono::milliseconds{10});
        }

        BOOST_CHECK(message == readMessage);
    }
    // handle annoying deadlock because made in haste
    *exitReader = true;
    while (!writeMessages->push(std::vector<uint8_t>(3)))
    {
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
    }
    *exitWriter = true;

    reader.join();
    writer.join();
}

BOOST_AUTO_TEST_SUITE_END()

}; // namespace ipc_test
