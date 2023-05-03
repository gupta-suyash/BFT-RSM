#include <boost/test/unit_test.hpp>

#include "acknowledgment_tracker.h"

namespace acknowledgment_tracker_test
{

BOOST_AUTO_TEST_SUITE(acknowledgment_tracker_test)

BOOST_AUTO_TEST_CASE(test_empty_tracker)
{
    AcknowledgmentTracker tracker(10, 5);
    BOOST_CHECK_EQUAL(0, tracker.getActiveResendData().isActive);
}

BOOST_AUTO_TEST_CASE(test_missing_message_zero)
{
    AcknowledgmentTracker tracker(4, 1);

    for (uint64_t node = 0; node < 4; node++)
    {
        tracker.update(node, 1, std::nullopt, std::nullopt);
    }
    BOOST_CHECK(0 == tracker.getActiveResendData().isActive);

    for (uint16_t update = 1; update <= 1000; update++)
    {
        for (uint64_t node = 0; node < 4; node++)
        {
            tracker.update(node, 1, std::nullopt, std::nullopt);
        }
        const auto expectedResendData = acknowledgment_tracker::ResendData{.sequenceNumber = 0, .resendNumber = update, .isActive = 1};
        BOOST_CHECK(expectedResendData == tracker.getActiveResendData());
    }
}

BOOST_AUTO_TEST_CASE(test_byzantine_attack)
{
    AcknowledgmentTracker tracker(4, 1);

    // node 0 is byzantine, they can change their ack, not my quorumAck
    tracker.update(0, 1, 0, std::nullopt);

    for (uint64_t node = 0; node < 4; node++)
    {
        tracker.update(node, 1, std::nullopt, std::nullopt);
    }
    BOOST_CHECK(0 == tracker.getActiveResendData().isActive);

    for (uint16_t update = 1; update <= 1000; update++)
    {
        for (uint64_t node = 0; node < 4; node++)
        {
            tracker.update(node, 1, std::nullopt, std::nullopt);
        }
        const auto expectedResendData = acknowledgment_tracker::ResendData{.sequenceNumber = 0, .resendNumber = update, .isActive = 1};
        BOOST_CHECK(expectedResendData == tracker.getActiveResendData());
    }
}

BOOST_AUTO_TEST_CASE(test_crash_attack)
{
    AcknowledgmentTracker tracker(4, 1);

    for (uint64_t node = 2; node < 4; node++)
    {
        tracker.update(node, 1, std::nullopt, std::nullopt);
    }
    BOOST_CHECK(0 == tracker.getActiveResendData().isActive);

    for (uint16_t update = 1; update <= 1000; update++)
    {
        for (uint64_t node = 2; node < 4; node++)
        {
            tracker.update(node, 1, std::nullopt, std::nullopt);
        }
        const auto expectedResendData = acknowledgment_tracker::ResendData{.sequenceNumber = 0, .resendNumber = update, .isActive = 1};
        BOOST_CHECK(expectedResendData == tracker.getActiveResendData());
    }
}

BOOST_AUTO_TEST_CASE(test_quadratic_updates)
{
    AcknowledgmentTracker tracker(4, 1);

    std::optional<uint64_t> quorumAck{std::nullopt};
    for (std::optional<uint32_t> sequenceNumber{}; sequenceNumber < 1000;
         sequenceNumber = sequenceNumber.value_or(-1) + 1)
    {
        tracker.update(0, 1, sequenceNumber, quorumAck);
        quorumAck = sequenceNumber; // after 2 updates quorumAck should be set

        tracker.update(1, 1, sequenceNumber, quorumAck);
        BOOST_CHECK(0 == tracker.getActiveResendData().isActive);

        tracker.update(2, 1, sequenceNumber, quorumAck);
        BOOST_CHECK(0 == tracker.getActiveResendData().isActive);

        tracker.update(3, 1, sequenceNumber, quorumAck);
        BOOST_CHECK(0 == tracker.getActiveResendData().isActive);

        for (uint16_t update = 1; update <= 1000; update++) // we're stuck at curQuorumAck
        {
            tracker.update(0, 1, sequenceNumber, quorumAck);
            tracker.update(1, 1, sequenceNumber, quorumAck);
            tracker.update(2, 1, sequenceNumber, quorumAck);
            tracker.update(3, 1, sequenceNumber, quorumAck);

            const auto expectedResendData = acknowledgment_tracker::ResendData{
                .sequenceNumber = sequenceNumber.value_or(-1) + 1, .resendNumber = update, .isActive = 1};
            BOOST_CHECK(expectedResendData == tracker.getActiveResendData());
        }
    }
}

BOOST_AUTO_TEST_CASE(test_quadratic_updates_byzantine)
{
    AcknowledgmentTracker tracker(4, 1);

    std::optional<uint64_t> quorumAck{std::nullopt};

    // node 0 is byzantine, they can change their ack, not my quorumAck
    tracker.update(0, 1, 0, quorumAck);

    for (std::optional<uint32_t> sequenceNumber{}; sequenceNumber < 1000;
         sequenceNumber = sequenceNumber.value_or(-1) + 1)
    {
        tracker.update(0, 1, sequenceNumber, quorumAck);
        quorumAck = sequenceNumber; // after 2 updates quorumAck should be set

        tracker.update(1, 1, sequenceNumber, quorumAck);
        BOOST_CHECK(0 == tracker.getActiveResendData().isActive);

        tracker.update(2, 1, sequenceNumber, quorumAck);
        BOOST_CHECK(0 == tracker.getActiveResendData().isActive);

        tracker.update(3, 1, sequenceNumber, quorumAck);
        BOOST_CHECK(0 == tracker.getActiveResendData().isActive);

        for (uint16_t update = 1; update <= 1000; update++) // we're stuck at curQuorumAck
        {
            tracker.update(0, 1, sequenceNumber, quorumAck);
            tracker.update(1, 1, sequenceNumber, quorumAck);
            tracker.update(2, 1, sequenceNumber, quorumAck);
            tracker.update(3, 1, sequenceNumber, quorumAck);

            const auto expectedResendData = acknowledgment_tracker::ResendData{
                .sequenceNumber = sequenceNumber.value_or(-1) + 1, .resendNumber = update, .isActive = 1};
            BOOST_CHECK(expectedResendData == tracker.getActiveResendData());
        }
    }
}

BOOST_AUTO_TEST_CASE(test_good_case)
{
    AcknowledgmentTracker tracker(4, 1);

    std::optional<uint64_t> quorumAck{std::nullopt};

    for (std::optional<uint32_t> sequenceNumber{}; sequenceNumber < 1000;
         sequenceNumber = sequenceNumber.value_or(-1) + 1)
    {
        tracker.update(0, 1, sequenceNumber, quorumAck);
        BOOST_CHECK(0 == tracker.getActiveResendData().isActive);
        quorumAck = sequenceNumber; // after 2 updates quorumAck should be set

        tracker.update(1, 1, sequenceNumber, quorumAck);
        BOOST_CHECK(0 == tracker.getActiveResendData().isActive);

        tracker.update(2, 1, sequenceNumber, quorumAck);
        BOOST_CHECK(0 == tracker.getActiveResendData().isActive);

        tracker.update(3, 1, sequenceNumber, quorumAck);
        BOOST_CHECK(0 == tracker.getActiveResendData().isActive);
    }
}

BOOST_AUTO_TEST_SUITE_END()

}; // namespace acknowledgment_tracker_test
