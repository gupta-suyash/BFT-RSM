#include <boost/test/unit_test.hpp>

#include <acknowledgment.h>
#include <array>
#include <scrooge_message.pb.h>

namespace acknowledgment_test
{

constexpr auto kViewSize = (1<<14)+64;

BOOST_AUTO_TEST_SUITE(acknowledgment_test)

BOOST_AUTO_TEST_CASE(test_empty_ack)
{
    Acknowledgment ack;
    const auto ackView = ack.getAckView<kViewSize>();
    BOOST_CHECK(ack.getAckIterator() == std::nullopt);
    BOOST_CHECK(acknowledgment::getAckIterator(ackView) == std::nullopt);
    BOOST_CHECK(acknowledgment::getFinalAck(ackView) == 0);
}

BOOST_AUTO_TEST_CASE(test_useless_acks)
{
    Acknowledgment ack;
    std::array bigAckValues{10, 11, 12, 13, 15, 20, 54, 62, 10000};
    for (const auto &x : bigAckValues)
    {
        ack.addToAckList(x);
        BOOST_CHECK(ack.getAckIterator() == std::nullopt);
        const auto ackView = ack.getAckView<kViewSize>();
        BOOST_CHECK(ackView.ackOffset == 1);
        BOOST_CHECK(acknowledgment::testAckView(ackView, x));
        BOOST_CHECK_EQUAL(acknowledgment::getFinalAck(ackView), x);
        BOOST_CHECK(acknowledgment::getAckIterator(ackView) == std::nullopt);
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
        const auto ackView = ack.getAckView<kViewSize>();
        BOOST_CHECK(acknowledgment::getAckIterator(ackView) == i);
        BOOST_CHECK(ackView.ackOffset == i+2);
        for (uint64_t ack = std::max<uint64_t>(0,i-100); ack <= i; ack++)
        {
            BOOST_CHECK(acknowledgment::testAckView(ackView, ack));
        }
        for (int noAck = i + 1; noAck < i+100; noAck++)
        {
            BOOST_CHECK(not acknowledgment::testAckView(ackView, noAck));
        }
        BOOST_CHECK_EQUAL(acknowledgment::getFinalAck(ackView), i + 1);
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
        const auto ackView = ack.getAckView<kViewSize>();
        BOOST_CHECK(ackView.ackOffset == 1);
        BOOST_CHECK(acknowledgment::testAckView(ackView, i));
        BOOST_CHECK_EQUAL(acknowledgment::getFinalAck(ackView), jumpSize);
        BOOST_CHECK(acknowledgment::getAckIterator(ackView) == std::nullopt);
    }

    ack.addToAckList(0);
    BOOST_CHECK(ack.getAckIterator() == jumpSize);

    const auto ackView = ack.getAckView<kViewSize>();
    BOOST_CHECK(acknowledgment::getAckIterator(ackView) == jumpSize);
    BOOST_CHECK(ackView.ackOffset == jumpSize + 2);
    BOOST_CHECK_EQUAL(acknowledgment::getFinalAck(ackView), jumpSize+1);

    for (uint64_t i = 0; i <= jumpSize; i++)
    {
        BOOST_CHECK(acknowledgment::testAckView(ackView, i));
    }

    BOOST_CHECK(not acknowledgment::testAckView(ackView, jumpSize + 1));

    for (uint64_t i = 2 * jumpSize; i <= cases; i += jumpSize)
    {
        const auto nextAckValue = i;
        const auto lastAckValue = i - jumpSize;
        for (uint64_t j = nextAckValue; j > lastAckValue + 1; j--)
        {
            // Check that adding non-consecutive acks does nothing
            ack.addToAckList(j);
            BOOST_CHECK(ack.getAckIterator() == lastAckValue);
            const auto ackView = ack.getAckView<kViewSize>();
            BOOST_CHECK(ackView.ackOffset == lastAckValue + 2);
            BOOST_CHECK(acknowledgment::testAckView(ackView, j));
            BOOST_CHECK(acknowledgment::testAckView(ackView, lastAckValue));
            BOOST_CHECK(acknowledgment::getAckIterator(ackView) == lastAckValue);
            BOOST_CHECK_EQUAL(acknowledgment::getFinalAck(ackView), nextAckValue);
        }
        // Check that connecting ack sequences makes the ack count jump up
        ack.addToAckList(lastAckValue + 1);
        BOOST_CHECK(ack.getAckIterator() == nextAckValue);
    }
}

static void setAckValue(scrooge::CrossChainMessage *const message, const acknowledgment::AckView<kViewSize> &curAckView)
{
    const auto ackIterator = acknowledgment::getAckIterator(curAckView);
    if (ackIterator.has_value())
    {
        message->mutable_ack_count()->set_value(ackIterator.value());
    }

    *message->mutable_ack_set() = {curAckView.view.begin(), curAckView.view.end()};
}

BOOST_AUTO_TEST_CASE(test_serilization)
{
    scrooge::CrossChainMessage msg;
    auto trueAckView = acknowledgment::AckView<kViewSize>{
        .ackOffset = 12345,
        .view = {}
    };
    uint64_t curVal = 1526354;
    for (auto& x : trueAckView.view)
    {
        x = curVal;
        curVal *= curVal;
    }

    setAckValue(&msg, trueAckView);

    auto resultAckView = acknowledgment::AckView<kViewSize>{};

    const auto curForeignAck = (msg.has_ack_count())
        ? std::optional<uint64_t>(msg.ack_count().value())
        : std::nullopt;
    std::copy_n(msg.ack_set().begin(),
                resultAckView.view.size(),
                resultAckView.view.begin()
            );
    resultAckView.ackOffset = curForeignAck.value_or(0ULL - 1ULL) + 2;

    BOOST_CHECK_EQUAL(resultAckView.ackOffset, trueAckView.ackOffset);

    for (uint64_t i = 0; i < trueAckView.view.size(); i++)
    {
        BOOST_CHECK_EQUAL(resultAckView.view[i], trueAckView.view[i]);
    }
}

BOOST_AUTO_TEST_SUITE_END()

} // namespace acknowledgment_test
