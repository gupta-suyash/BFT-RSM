// Hi

#include <spdlog/spdlog.h>
#include <atomic>
#include <thread>
#include <chrono>
#include "../systems/readerwritercircularbuffer.h"

int main(int argc, char *argv[])
{
    using namespace std::chrono_literals;
    std::atomic_bool shouldStop{};
    moodycamel::ReaderWriterCircularBuffer<scrooge::CrossChainMessageData> queue(1024);

    auto x = std::thread([&](){
        uint64_t sn{};
        while (not shouldStop)
        {
            scrooge::CrossChainMessageData elem;
            
            elem.set_message_content(std::string(100, 'L'));
            elem.set_sequence_number(sn++);
            while (not queue.try_enqueue(std::move(elem)) && not shouldStop);
        }
    });

    auto y = std::thread([&](){
        uint64_t sn{};
        while (not shouldStop)
        {
            scrooge::CrossChainMessageData elem;
            sn += queue.try_dequeue(elem);
        }
        SPDLOG_CRITICAL("COULD POP {} ELEMENTS", sn);
    });

    std::this_thread::sleep_for(10s);
    shouldStop = true;
    x.join();
    y.join();
}
