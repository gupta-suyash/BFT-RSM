#include <boost/test/unit_test.hpp>

#include "acknowledgment_tracker.h"

namespace acknowledgment_tracker_test
{
using namespace std::chrono_literals;
const std::chrono::steady_clock::time_point kTestStartTime{1'000'000s};
const std::chrono::steady_clock::time_point kInvalidTime{kTestStartTime - 1s};

BOOST_AUTO_TEST_SUITE(acknowledgment_tracker_test)

BOOST_AUTO_TEST_CASE(test_empty_tracker)
{
    AcknowledgmentTracker tracker{};
    for (uint64_t ack = 0; ack < 1000; ack++)
    {
        BOOST_CHECK(tracker.getAggregateInitialAckTime(ack) == std::nullopt);
        BOOST_CHECK_EQUAL(tracker.getAggregateRepeatedAckCount(ack), 0);
    }
}

BOOST_AUTO_TEST_CASE(test_non_aggregate_queries)
{
    constexpr uint64_t kNumNodes = 100;
    constexpr auto stuckAmt = [](uint64_t nodeId) {
        const auto lowBits = nodeId & 0b1111;
        return 200 - lowBits;
    };

    AcknowledgmentTracker tracker{};

    for (uint64_t node = 0; node < kNumNodes; node++)
    {
        const auto initStuckTime = kTestStartTime + std::chrono::milliseconds{node};
        const auto stuckAck = node;
        const auto stuckAmount = stuckAmt(node);
        for (uint64_t update = 1; update <= stuckAmount; update++)
        {
            const auto updateTime = initStuckTime + std::chrono::seconds{update - 1};
            tracker.updateNodeData(node, stuckAck, updateTime);
            const auto trackerInitTime = tracker.getAggregateInitialAckTime(stuckAck);
            const auto trackerRepeatedAckCount = tracker.getAggregateRepeatedAckCount(stuckAck);
            BOOST_CHECK(initStuckTime == trackerInitTime);
            BOOST_CHECK_EQUAL(update, trackerRepeatedAckCount);
        }
    }
}

BOOST_AUTO_TEST_CASE(test_aggregate_queries)
{
    constexpr uint64_t kNumNodes = 100;
    constexpr uint64_t kNumNodeCopies = 10;
    constexpr auto stuckAmt = [](uint64_t nodeId) {
        const auto lowBits = nodeId & 0b1111;
        return 200 - lowBits;
    };

    AcknowledgmentTracker tracker{};
    for (uint64_t node = 0; node < kNumNodes; node++)
    {
        for (uint64_t copy = 0; copy < kNumNodeCopies; copy++)
        {
            const auto nodeId = node * kNumNodeCopies + copy;
            const auto initStuckTime =
                kTestStartTime + std::chrono::milliseconds{node + 1} - std::chrono::microseconds{copy};
            const auto stuckAck = node;
            const auto stuckAmount = stuckAmt(node);
            for (uint64_t update = 1; update <= stuckAmount; update++)
            {
                const auto updateTime = initStuckTime + std::chrono::seconds{update - 1};
                tracker.updateNodeData(nodeId, stuckAck, updateTime);
                const auto trackerInitTime = tracker.getAggregateInitialAckTime(stuckAck);
                const auto trackerRepeatedAckCount = tracker.getAggregateRepeatedAckCount(stuckAck);
                const auto minimumStuckTime = initStuckTime;
                const auto totalRepeatedAckCount = stuckAmount * copy + update;
                BOOST_CHECK(minimumStuckTime == trackerInitTime);
                BOOST_CHECK_EQUAL(totalRepeatedAckCount, trackerRepeatedAckCount);
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(test_aggregate_updates)
{
    constexpr uint64_t kNumNodes = 100;
    constexpr uint64_t kNumNodeCopies = 10;
    constexpr uint64_t kNumUpdates = 10;
    constexpr auto stuckAmt = [](uint64_t nodeId) {
        const auto lowBits = nodeId & 0b1111;
        return 100 - lowBits;
    };

    AcknowledgmentTracker tracker{};

    for (uint64_t nodeUpdate = 0; nodeUpdate < kNumUpdates; nodeUpdate++)
    {
        for (uint64_t node = 0; node < kNumNodes; node++)
        {
            for (uint64_t copy = 0; copy < kNumNodeCopies; copy++)
            {
                const auto nodeId = node * kNumNodeCopies + copy;
                const auto initStuckTime = kTestStartTime + std::chrono::milliseconds{node + 1} -
                                           std::chrono::microseconds{copy} + std::chrono::nanoseconds{nodeUpdate};
                const auto stuckAck = kNumNodes * nodeUpdate + node;
                const auto stuckAmount = stuckAmt(node) + nodeUpdate;
                for (uint64_t update = 1; update <= stuckAmount; update++)
                {
                    const auto updateTime = initStuckTime + std::chrono::seconds{update - 1};
                    tracker.updateNodeData(nodeId, stuckAck, updateTime);
                    const auto trackerInitTime = tracker.getAggregateInitialAckTime(stuckAck);
                    const auto trackerRepeatedAckCount = tracker.getAggregateRepeatedAckCount(stuckAck);
                    const auto minimumStuckTime = initStuckTime;
                    const auto totalRepeatedAckCount = stuckAmount * copy + update;
                    BOOST_CHECK(minimumStuckTime == trackerInitTime);
                    BOOST_CHECK_EQUAL(totalRepeatedAckCount, trackerRepeatedAckCount);
                }
            }
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()

}; // namespace acknowledgment_tracker_test
