#include <boost/test/unit_test.hpp>

#include <acknowledgment.h>
#include <array>

namespace acknowledgment_test
{

BOOST_AUTO_TEST_SUITE(acknowledgment_test)

BOOST_AUTO_TEST_CASE(test_empty_ack)
{
    Acknowledgment ack;
    BOOST_CHECK(ack.getAckIterator() == std::nullopt);
}

BOOST_AUTO_TEST_CASE(test_useless_acks)
{
    Acknowledgment ack;
    std::array bigAckValues{10, 11, 12, 13, 15, 20, 62, 54, 10000};
    for (const auto &x : bigAckValues)
    {
        ack.addToAckList(x);
        BOOST_CHECK(ack.getAckIterator() == std::nullopt);
        const auto ackView = ack.getAckView<(1 << 14)>();
        BOOST_CHECK(ackView.ackOffset == 0);
        BOOST_CHECK(acknowledgment::testAckView(ackView, x));
    }
}

BOOST_AUTO_TEST_CASE(test_consecutive_acks)
{
    constexpr auto kCases = 10000;
    Acknowledgment ack;
    for (int i = 0; i <= kCases; i++)
    {
        ack.addToAckList(i);
        BOOST_CHECK(ack.getAckIterator() == i);
        const auto ackView = ack.getAckView<(1 << 14)>();
        BOOST_CHECK(ackView.ackOffset == i+1);
        BOOST_CHECK(ackView.view[0] == 0);
    }
}

BOOST_AUTO_TEST_CASE(test_nonconsecutive_acks)
{
    constexpr auto cases = 10000;
    constexpr auto jumpSize = 10;
    Acknowledgment ack;

    for (uint64_t i = jumpSize; i > 0; i--)
    {
        ack.addToAckList(i);
        BOOST_CHECK(ack.getAckIterator() == std::nullopt);
        const auto ackView = ack.getAckView<(1 << 14)>();
        BOOST_CHECK(ackView.ackOffset == 0);
        BOOST_CHECK(acknowledgment::testAckView(ackView, i));
    }

    ack.addToAckList(0);
    BOOST_CHECK(ack.getAckIterator() == jumpSize);

    for (uint64_t i = 2 * jumpSize; i <= cases; i += jumpSize)
    {
        const auto nextAckValue = i;
        const auto lastAckValue = i - jumpSize;
        for (uint64_t j = nextAckValue; j > lastAckValue + 1; j--)
        {
            // Check that adding non-consecutive acks does nothing
            ack.addToAckList(j);
            BOOST_CHECK(ack.getAckIterator() == lastAckValue);
        }
        // Check that connecting ack sequences makes the ack count jump up
        ack.addToAckList(lastAckValue + 1);
        BOOST_CHECK(ack.getAckIterator() == nextAckValue);
    }
}

BOOST_AUTO_TEST_SUITE_END()

} // namespace acknowledgment_test
