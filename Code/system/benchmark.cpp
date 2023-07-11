#include "benchmark.h"

uint64_t test_integer(uint64_t max_power) {
    using namespace std::chrono_literals;
    std::cout << "---------------------- STARTING QUEUE INTEGER TESTS ---------------------- " << std::endl;
    for (size_t i = min_size_power; i <= max_power; i++) {
        std::atomic_bool shouldStop{};
        moodycamel::ReaderWriterQueue<uint64_t> queue(1024);

        auto x = std::thread([&](){
            // std::cout << "Send Thread starting with TID = {}", gettid());
            uint64_t sn{1};
            while (not shouldStop.load(std::memory_order_relaxed))
            {
                sn++;
                while (not queue.try_enqueue(sn) && not shouldStop.load(std::memory_order_relaxed));
            }
        });

        auto y = std::thread([&](){
            // std::cout << "Receive Thread starting with TID = {}", gettid());
            uint64_t other_sn{};
            while (not shouldStop.load(std::memory_order_relaxed))
            {
                uint64_t recv_sn{0};
                other_sn += queue.try_dequeue(recv_sn);
            }
            std::cout << "COULD POP " << other_sn << "  ELEMENTS, total rate of "<< other_sn/duration<<" elements/second " << std::endl;
        });

        std::this_thread::sleep_for(10s);
        shouldStop = true;
        x.join();
        y.join();
        std::cout << "Completed test_size test for size " << i << std::endl;
    }
    std::cout << "Completed size test for integers " << std::endl;
    return 0;
}

uint64_t test_stack_protobuf_queue_rate(uint64_t max_power, uint64_t msg_sz) {
    using namespace std::chrono_literals;
    SPDLOG_CRITICAL("---------------------- STARTING PROTOBUF QUEUE STACK TESTS WITH POWER {} and SIZE {} ---------------------- ", max_power, msg_sz);
    for (size_t i = min_size_power; i <= max_power; i++) {
        std::atomic_bool shouldStop{};
        moodycamel::ReaderWriterQueue<scrooge::CrossChainMessageData> queue(2 << i);
        auto x = std::thread([&](){
            SPDLOG_CRITICAL("STACK SEND THREAD starting with TID = {}", gettid());
            uint64_t sn{};
            while (not shouldStop)
            {
                scrooge::CrossChainMessageData elem;
                elem.set_message_content(std::string(msg_sz, 'L'));
                elem.set_sequence_number(sn++);
                while (not queue.try_enqueue(std::move(elem)) && not shouldStop);
            }
        });

        auto y = std::thread([&](){
            SPDLOG_CRITICAL("STACK RECEIVE THREAD starting with TID = {}", gettid());
            uint64_t sn{};
            while (not shouldStop)
            {
                scrooge::CrossChainMessageData elem;
                sn += queue.try_dequeue(elem);
            }
            std::cout << "COULD POP " << sn << "  ELEMENTS, total rate of "<< sn/duration<<" elements/second for size " << msg_sz << std::endl;
        });

        std::this_thread::sleep_for(10s);
        shouldStop = true;
        x.join();
        y.join();
        std::cout << "Completed test_size test for size " << i << std::endl;
    }
    std::cout << "Completed size test for protobuf " << std::endl;
    return 0;
}

uint64_t test_heap_protobuf_queue_rate(uint64_t max_power, uint64_t msg_sz) {
    using namespace std::chrono_literals;
    SPDLOG_CRITICAL("---------------------- STARTING PROTOBUF QUEUE HEAP TESTS WITH POWER {} and SIZE {} ---------------------- ", max_power, msg_sz);
    for (size_t i = min_size_power; i <= max_power; i++) {
        std::atomic_bool shouldStop{};
        moodycamel::ReaderWriterQueue<std::unique_ptr<scrooge::CrossChainMessageData>> queue(2 << i);

        auto x = std::thread([&](){
            SPDLOG_CRITICAL("HEAP SEND THREAD starting with TID = {}", gettid());
            
            uint64_t sn{};
            while (not shouldStop)
            {
                auto elem = std::make_unique<scrooge::CrossChainMessageData>();
                elem->set_message_content(std::string(msg_sz, 'L'));
                elem->set_sequence_number(sn++);
                while (not queue.try_enqueue(std::move(elem)) && not shouldStop);
            }
        });

        auto y = std::thread([&](){
            SPDLOG_CRITICAL("HEAP RECEIVE THREAD starting with TID = {}", gettid());
            uint64_t sn{};
            while (not shouldStop)
            {
                std::unique_ptr<scrooge::CrossChainMessageData> elem;
                sn += queue.try_dequeue(elem);
            }
            std::cout << "COULD POP " << sn << "  ELEMENTS, total rate of "<< sn/duration<<" elements/second " << std::endl;
        });

        std::this_thread::sleep_for(10s);
        shouldStop = true;
        x.join();
        y.join();
        std::cout << "Completed test_size test for size " << i << std::endl;
    }
    std::cout << "Completed size test for protobuf heap" << std::endl;
    return 0; 
}

// uint64_t test_protobuf_creation_stack(uint64_t string_sz) {
//     using namespace std::chrono_literals;
//     std::cout << "---------------------- STARTING PROTOBUF CREATION STACK TESTS ---------------------- " << std::endl;
//     std::atomic_bool shouldStop{};

//     auto x = std::thread([&](){
//         // std::cout << "Send Thread starting with TID = {}", gettid());
//         uint64_t sn{};
//         while (not shouldStop)
//         {
//             test::TestMessage elem;
            
//             elem.set_message_content(std::string(string_sz, 'L'));
//             elem.set_sequence_number(sn++);
//         }
//         std::cout << "COULD CREATE "<< sn << " ELEMENTS, total rate of " << sn/duration << " elements/second" << std::endl;
//     });

//     std::this_thread::sleep_for(10s);
//     shouldStop = true;
//     x.join();
//     std::cout << "Completed size test for protobuf " << std::endl;
//     return 0;
// }

// uint64_t test_protobuf_creation_heap(uint64_t string_sz) {
//     using namespace std::chrono_literals;
//     std::cout << "---------------------- STARTING PROTOBUF CREATION HEAP TESTS ---------------------- " << std::endl;
//     std::atomic_bool shouldStop{};

//     std::cout << "---------------------- ARENA ---------------------- " << std::endl;
//     auto x = std::thread([&](){
//         // std::cout << "Send Thread starting with TID = {}", gettid());
//         uint64_t sn{};
//         google::protobuf::Arena arena;
//         while (not shouldStop)
//         {
//             test::TestMessage* elem = google::protobuf::Arena::CreateMessage<test::TestMessage>(&arena);
//             elem->set_message_content(std::string(string_sz, 'L'));
//             elem->set_sequence_number(sn++);
//         }
//         std::cout << "COULD CREATE "<<sn << "  ELEMENTS, total rate of "<< sn/duration << " elements/second" << std::endl;
//     });

//     std::this_thread::sleep_for(10s);
//     shouldStop = true;
//     x.join();
//     std::cout << "Completed size test for protobuf " << std::endl;

//     std::cout << "---------------------- ARENA ---------------------- " << std::endl;
//     auto y = std::thread([&](){
//         // std::cout << "Send Thread starting with TID = {}", gettid());
//         uint64_t sn{};
//         google::protobuf::Arena arena;
//         while (not shouldStop)
//         {
//             test::TestMessage* elem = google::protobuf::Arena::CreateMessage<test::TestMessage>(&arena);
//             elem->set_message_content(std::string(string_sz, 'L'));
//             elem->set_sequence_number(sn++);
//         }
//         std::cout << "COULD CREATE " << sn << " ELEMENTS, total rate of " << sn/duration << " elements/second"  << std::endl;
//     });

//     std::this_thread::sleep_for(10s);
//     shouldStop = true;
//     y.join();
//     std::cout << "Completed size test for protobuf " << std::endl;
//     return 0;
// }





