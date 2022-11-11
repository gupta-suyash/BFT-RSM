#include <boost/test/unit_test.hpp>

#include <acknowledgement.h>
#include <array>

namespace acknowledgement_test
{

BOOST_AUTO_TEST_SUITE(acknowledgement_test)

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
    }
}

BOOST_AUTO_TEST_CASE(test_consecutive_acks)
{
    constexpr auto kCases = 10000;
    Acknowledgment ack;
    for (int i = 1; i <= kCases; i++)
    {
        ack.addToAckList(i);
        BOOST_CHECK(ack.getAckIterator() == i);
    }
}

BOOST_AUTO_TEST_CASE(test_nonconsecutive_acks)
{
    constexpr auto cases = 10000;
    constexpr auto jumpSize = 10;
    Acknowledgment ack;

    for (uint64_t i = 1; i <= jumpSize; i++)
    {
        ack.addToAckList(i);
        BOOST_CHECK(ack.getAckIterator() == i);
    }

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

} // namespace acknowledgement_test
