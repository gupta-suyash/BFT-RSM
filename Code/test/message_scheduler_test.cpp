#include <boost/test/unit_test.hpp>

#include <message_scheduler.h>

namespace message_scheduler_test
{
const auto noFailuresNetworkConfiguraiton = NodeConfiguration{
    .kOwnNetworkSize = 2,
    .kOtherNetworkSize = 2,
    .kOwnNetworkStakes = std::vector<uint64_t>{1, 1},
    .kOtherNetworkStakes = std::vector<uint64_t>{1, 1},
    .kOwnMaxNumFailedStake = 0,
    .kOtherMaxNumFailedStake = 0,
    .kNodeId = 0,
    .kLogPath = "asdf",
    .kWorkingDir = "asdf",
};

const auto scaledNoFailuresNetworkConfiguraiton = NodeConfiguration{
    .kOwnNetworkSize = 2,
    .kOtherNetworkSize = 2,
    .kOwnNetworkStakes = std::vector<uint64_t>{1, 1},
    .kOtherNetworkStakes = std::vector<uint64_t>{10, 10},
    .kOwnMaxNumFailedStake = 0,
    .kOtherMaxNumFailedStake = 0,
    .kNodeId = 0,
    .kLogPath = "asdf",
    .kWorkingDir = "asdf",
};

const auto unevenNoFailuresNetworkConfiguraiton = NodeConfiguration{
    .kOwnNetworkSize = 2,
    .kOtherNetworkSize = 2,
    .kOwnNetworkStakes = std::vector<uint64_t>{3, 5},
    .kOtherNetworkStakes = std::vector<uint64_t>{10, 12345},
    .kOwnMaxNumFailedStake = 0,
    .kOtherMaxNumFailedStake = 0,
    .kNodeId = 0,
    .kLogPath = "asdf",
    .kWorkingDir = "asdf",
};

const auto pbftConfiguraiton = NodeConfiguration{
    .kOwnNetworkSize = 4,
    .kOtherNetworkSize = 4,
    .kOwnNetworkStakes = std::vector<uint64_t>{1, 1, 1, 1},
    .kOtherNetworkStakes = std::vector<uint64_t>{1, 1, 1, 1},
    .kOwnMaxNumFailedStake = 1,
    .kOtherMaxNumFailedStake = 1,
    .kNodeId = 0,
    .kLogPath = "asdf",
    .kWorkingDir = "asdf",
};

BOOST_AUTO_TEST_SUITE(message_scheduler_test)

BOOST_AUTO_TEST_CASE(test_prefix_sums)
{
    const auto totalStake = 11;
    const std::vector<uint64_t> stakes{1, 1, 1, 5, 2, 1};
    const std::vector<uint64_t> expectedStakes{0, 1, 2, 3, 8, 10, 11};
    const auto prefixSum = message_scheduler::getStakePrefixSum(stakes);

    BOOST_CHECK(expectedStakes == prefixSum);
    BOOST_CHECK_EQUAL(totalStake, message_scheduler::stakeInNetwork(prefixSum));

    uint64_t curStake = 0;
    for (uint64_t curNode = 0; curNode < stakes.size(); curNode++)
    {
        for (int i = 0; i < stakes.at(curNode); i++)
        {
            BOOST_CHECK_EQUAL(curNode, message_scheduler::stakeToNode(curStake, prefixSum));

            curStake++;
        }
    }
}

BOOST_AUTO_TEST_CASE(test_no_failures)
{
    MessageScheduler scheduler(noFailuresNetworkConfiguraiton);
    message_scheduler::CompactDestinationList expectedDestinations{1};
    for (int sequenceNumber = 0; sequenceNumber < 1000; sequenceNumber += 2)
    {
        expectedDestinations.at(0) = (sequenceNumber / 2) % 2;
        BOOST_CHECK(scheduler.getResendNumber(sequenceNumber) == 0);
        BOOST_CHECK(scheduler.getMessageDestinations(sequenceNumber) == expectedDestinations);
    }

    expectedDestinations.pop_back();
    for (int sequenceNumber = 1; sequenceNumber < 1000; sequenceNumber += 2)
    {

        BOOST_CHECK(scheduler.getResendNumber(sequenceNumber) == std::nullopt);
        BOOST_CHECK(scheduler.getMessageDestinations(sequenceNumber) == expectedDestinations);
    }
}

BOOST_AUTO_TEST_CASE(test_scaled_no_failures)
{
    MessageScheduler scheduler(scaledNoFailuresNetworkConfiguraiton);
    message_scheduler::CompactDestinationList expectedDestinations{1};
    for (int sequenceNumber = 0; sequenceNumber < 1000; sequenceNumber++)
    {
        if (sequenceNumber % 20 < 10)
        {
            const auto destinations = scheduler.getMessageDestinations(sequenceNumber);
            BOOST_CHECK(scheduler.getResendNumber(sequenceNumber) == 0);
            BOOST_CHECK_EQUAL(destinations.size(), 1);
            BOOST_CHECK(destinations.at(0) == 0 || destinations.at(0) == 1);
        }
        else
        {
            BOOST_CHECK(scheduler.getResendNumber(sequenceNumber) == std::nullopt);
            const auto destinations = scheduler.getMessageDestinations(sequenceNumber);
            BOOST_CHECK_EQUAL(destinations.size(), 0);
        }
    }
}

BOOST_AUTO_TEST_CASE(test_pbft)
{
    MessageScheduler scheduler(pbftConfiguraiton);
    message_scheduler::CompactDestinationList expectedDestinations{2};
    for (int sequenceNumber = 0; sequenceNumber < 1000; sequenceNumber++)
    {
        if (sequenceNumber % 4 == 0)
        {
            const auto destinations = scheduler.getMessageDestinations(sequenceNumber);
            BOOST_CHECK(scheduler.getResendNumber(sequenceNumber) == 0);
            BOOST_CHECK_EQUAL(destinations.size(), 1);
            BOOST_CHECK_EQUAL(destinations.at(0), (sequenceNumber / 4) % 4);
        }
        else if (sequenceNumber % 4 == 3)
        {
            const auto destinations = scheduler.getMessageDestinations(sequenceNumber);
            BOOST_CHECK(scheduler.getResendNumber(sequenceNumber) == 1);
            BOOST_CHECK_EQUAL(destinations.size(), 1);
            BOOST_CHECK_EQUAL(destinations.at(0), (sequenceNumber / 4) % 4);
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()

} // namespace message_scheduler_test
