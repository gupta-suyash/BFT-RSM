#include <boost/test/unit_test.hpp>

#include <quorum_acknowledgement.h>

namespace quorum_acknowledgement_test
{

BOOST_AUTO_TEST_SUITE(quorum_acknowledgement_test)

constexpr uint64_t kTestQuorumSize = 50;
constexpr uint64_t kTestNetworkSize = 3 * kTestQuorumSize + 1;

BOOST_AUTO_TEST_CASE(test_empty_quack)
{
    QuorumAcknowledgment quack{kTestQuorumSize};
    BOOST_CHECK(quack.getCurrentQuack() == std::nullopt);
}

BOOST_AUTO_TEST_CASE(test_useless_quacks)
{
    QuorumAcknowledgment quack{kTestQuorumSize};
    for (uint64_t node = 1; node < kTestQuorumSize; node++)
    {
        quack.updateNodeAck(node, 10);
        BOOST_CHECK(quack.getCurrentQuack() == std::nullopt);
    }
}

BOOST_AUTO_TEST_CASE(test_useful_quacks)
{
    const auto getNode = [&](const auto nodeOffset) {
        constexpr int64_t curNodeGenerator = 79; // gcd(curNodeGen, kTestNetworkSize) == 1
        return (nodeOffset * curNodeGenerator) % kTestNetworkSize + 1;
    };
    int64_t curNodeOffset = 0;
    for (uint64_t initAck = 1; initAck <= 50; initAck++)
    {
        QuorumAcknowledgment quack{kTestQuorumSize};
        for (uint64_t node_offset = 0; node_offset < kTestQuorumSize - 1; node_offset++)
        {
            const auto currentNode = getNode(curNodeOffset++);
            quack.updateNodeAck(currentNode, initAck);
            BOOST_CHECK(quack.getCurrentQuack() == std::nullopt);
        }
        const auto finalNode = getNode(curNodeOffset++);
        quack.updateNodeAck(finalNode, initAck);
        BOOST_CHECK(quack.getCurrentQuack() == initAck);
    }
}

BOOST_AUTO_TEST_CASE(test_monotonicity)
{
    QuorumAcknowledgment quack{kTestQuorumSize};
    for (uint64_t node = 1; node < kTestQuorumSize; node++)
    {
        quack.updateNodeAck(node, 10);
        BOOST_CHECK(quack.getCurrentQuack() == std::nullopt);
    }
    quack.updateNodeAck(kTestQuorumSize, 10);
    for (uint64_t node = 1; node <= kTestNetworkSize; node++)
    {
        quack.updateNodeAck(node, 9);
        BOOST_CHECK(quack.getCurrentQuack() == 10);
    }
}

BOOST_AUTO_TEST_CASE(test_nonconsecutive_quacks)
{
    constexpr auto cases = 10000;
    constexpr auto jumpSize = 10;
    const auto getNode = [&](const auto nodeOffset) {
        constexpr int64_t curNodeGenerator = 79; // gcd(curNodeGen, kTestNetworkSize) == 1
        return (nodeOffset * curNodeGenerator) % kTestNetworkSize + 1;
    };
    int64_t curNodeOffset = 0;
    QuorumAcknowledgment quack{kTestQuorumSize};
    for (uint64_t i = 1; i < kTestQuorumSize; i++)
    {
        quack.updateNodeAck(getNode(curNodeOffset++), jumpSize);
        BOOST_CHECK(quack.getCurrentQuack() == std::nullopt);
    }
    quack.updateNodeAck(getNode(curNodeOffset++), jumpSize);
    BOOST_CHECK(quack.getCurrentQuack() == jumpSize);

    for (uint64_t i = 2 * jumpSize; i <= cases; i += jumpSize)
    {
        const auto lastQuack = i - jumpSize;
        const auto nextQuack = i;
        for (uint64_t j = 1; j < kTestQuorumSize; j++)
        {
            quack.updateNodeAck(getNode(curNodeOffset++), nextQuack);
            BOOST_CHECK(quack.getCurrentQuack() == lastQuack);
        }
        quack.updateNodeAck(getNode(curNodeOffset++), nextQuack);
        BOOST_CHECK(quack.getCurrentQuack() == nextQuack);
    }
}

BOOST_AUTO_TEST_SUITE_END()

}; // namespace quorum_acknowledgement_test
