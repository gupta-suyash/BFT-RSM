#include <boost/test/unit_test.hpp>

#include "acknowledgment.h"
#include "acknowledgment_tracker.h"

#include <algorithm>

namespace repeated_acknowledgment_tracker_test
{
constexpr auto kOtherNetworkSize = 4;
constexpr auto kOtherNetworkFailedStake = 1;

template <uint64_t kListSize>
void updateAckTrackers(const std::optional<uint64_t> curQuack, const uint64_t nodeId, const uint64_t nodeStake,
                       const acknowledgment::AckView<kListSize> nodeAckView,
                       std::vector<std::unique_ptr<AcknowledgmentTracker>> *const ackTrackers)
{
    const auto kNumAckTrackers = ackTrackers->size();
    const auto initialMessageTrack = curQuack.value_or(0ULL - 1ULL) + 1;
    const auto finialMessageTrack =
        std::min(initialMessageTrack + kNumAckTrackers - 1, acknowledgment::getFinalAck(nodeAckView));
    for (uint64_t curMessage = initialMessageTrack; curMessage <= finialMessageTrack; curMessage++)
    {
        const auto virtualQuack = (curMessage) ? std::optional<uint64_t>(curMessage - 1) : std::nullopt;
        const auto isNodeMissingCurMessage = not acknowledgment::testAckView(nodeAckView, curMessage);
        const auto curAckTracker = ackTrackers->data() + (curMessage % ackTrackers->size());

        if (isNodeMissingCurMessage)
        {
            (*curAckTracker)->update(nodeId, nodeStake, virtualQuack, virtualQuack);
        }
        else
        {
            (*curAckTracker)->update(nodeId, nodeStake, virtualQuack.value_or(0ULL - 1ULL) + 1, virtualQuack);
        }
    }
}

bool hasNoResends(std::vector<std::unique_ptr<AcknowledgmentTracker>> *const ackTrackers)
{
    return std::none_of(ackTrackers->begin(), ackTrackers->end(),
                        [](const auto &x) { return x->getActiveResendData().has_value(); });
}

bool hasResends(std::vector<std::unique_ptr<AcknowledgmentTracker>> *const ackTrackers)
{
    return std::any_of(ackTrackers->begin(), ackTrackers->end(),
                       [](const auto &x) { return x->getActiveResendData().has_value(); });
}

BOOST_AUTO_TEST_SUITE(repeated_acknowledgment_tracker_test)

BOOST_AUTO_TEST_CASE(test_single_tracker_perfect_case)
{
    constexpr auto kListSize = 0;
    constexpr auto kNumTrackers = 1;
    constexpr auto kStakePerNode = 1;
    constexpr auto kNumTests = 10'000;
    auto ackTrackers = std::vector<std::unique_ptr<AcknowledgmentTracker>>();
    for (uint64_t i = 0; i < kNumTrackers; i++)
    {
        ackTrackers.push_back(std::make_unique<AcknowledgmentTracker>(kOtherNetworkSize, kOtherNetworkFailedStake));
    }

    acknowledgment::AckView<kListSize> ackView{.ackOffset = 1, .view = {}};
    updateAckTrackers<kListSize>(std::nullopt, 0, kStakePerNode, ackView, &ackTrackers);
    BOOST_CHECK(hasNoResends(&ackTrackers));

    updateAckTrackers(std::nullopt, 1, kStakePerNode, ackView, &ackTrackers);
    BOOST_CHECK(hasNoResends(&ackTrackers));

    updateAckTrackers(std::nullopt, 2, kStakePerNode, ackView, &ackTrackers);
    BOOST_CHECK(hasNoResends(&ackTrackers));

    updateAckTrackers(std::nullopt, 3, kStakePerNode, ackView, &ackTrackers);
    BOOST_CHECK(hasNoResends(&ackTrackers));

    for (uint64_t curQuack{}; curQuack < kNumTests; curQuack++)
    {
        ackView.ackOffset = curQuack + 2;
        for (uint64_t curNode{}; curNode < kOtherNetworkSize; curNode++)
        {
            updateAckTrackers(curQuack, curNode, kStakePerNode, ackView, &ackTrackers);
            BOOST_CHECK(hasNoResends(&ackTrackers));
        }
    }
}

BOOST_AUTO_TEST_CASE(test_single_tracker_good_enough_case)
{
    constexpr auto kListSize = 0;
    constexpr auto kNumTrackers = 1;
    constexpr auto kStakePerNode = 1;
    constexpr auto kNumTests = 10'000;
    auto ackTrackers = std::vector<std::unique_ptr<AcknowledgmentTracker>>();
    for (uint64_t i = 0; i < kNumTrackers; i++)
    {
        ackTrackers.push_back(std::make_unique<AcknowledgmentTracker>(kOtherNetworkSize, kOtherNetworkFailedStake));
    }

    acknowledgment::AckView<kListSize> ackView{.ackOffset = 1, .view = {}};
    updateAckTrackers<kListSize>(std::nullopt, 0, kStakePerNode, ackView, &ackTrackers);
    BOOST_CHECK(hasNoResends(&ackTrackers));

    updateAckTrackers<kListSize>(std::nullopt, 0, kStakePerNode, ackView, &ackTrackers);
    BOOST_CHECK(hasNoResends(&ackTrackers));

    updateAckTrackers<kListSize>(std::nullopt, 0, kStakePerNode, ackView, &ackTrackers);
    BOOST_CHECK(hasNoResends(&ackTrackers));

    updateAckTrackers(std::nullopt, 1, kStakePerNode, ackView, &ackTrackers);
    BOOST_CHECK(hasNoResends(&ackTrackers));

    updateAckTrackers(std::nullopt, 2, kStakePerNode, ackView, &ackTrackers);
    BOOST_CHECK(hasNoResends(&ackTrackers));

    updateAckTrackers(std::nullopt, 3, kStakePerNode, ackView, &ackTrackers);
    BOOST_CHECK(hasNoResends(&ackTrackers));

    for (uint64_t curQuack{}; curQuack < kNumTests; curQuack++)
    {
        ackView.ackOffset = curQuack + 2;
        updateAckTrackers(curQuack, 0, kStakePerNode, ackView, &ackTrackers);
        BOOST_CHECK(hasNoResends(&ackTrackers));
        updateAckTrackers(curQuack, 0, kStakePerNode, ackView, &ackTrackers);
        BOOST_CHECK(hasNoResends(&ackTrackers));
        updateAckTrackers(curQuack, 0, kStakePerNode, ackView, &ackTrackers);
        BOOST_CHECK(hasNoResends(&ackTrackers));
        for (uint64_t curNode{}; curNode < kOtherNetworkSize; curNode++)
        {
            updateAckTrackers(curQuack, curNode, kStakePerNode, ackView, &ackTrackers);
            BOOST_CHECK(hasNoResends(&ackTrackers));
        }
    }
}

BOOST_AUTO_TEST_CASE(test_single_tracker_stuck_case)
{
    constexpr auto kListSize = 0;
    constexpr auto kNumTrackers = 1;
    constexpr auto kStakePerNode = 1;
    constexpr auto kNumTests = 10'000;
    auto ackTrackers = std::vector<std::unique_ptr<AcknowledgmentTracker>>();
    for (uint64_t i = 0; i < kNumTrackers; i++)
    {
        ackTrackers.push_back(std::make_unique<AcknowledgmentTracker>(kOtherNetworkSize, kOtherNetworkFailedStake));
    }

    acknowledgment::AckView<kListSize> ackView{.ackOffset = 1, .view = {}};
    updateAckTrackers<kListSize>(std::nullopt, 0, kStakePerNode, ackView, &ackTrackers);
    BOOST_CHECK(hasNoResends(&ackTrackers));

    updateAckTrackers<kListSize>(std::nullopt, 0, kStakePerNode, ackView, &ackTrackers);
    BOOST_CHECK(hasNoResends(&ackTrackers));

    updateAckTrackers(std::nullopt, 1, kStakePerNode, ackView, &ackTrackers);
    BOOST_CHECK(hasNoResends(&ackTrackers));

    updateAckTrackers(std::nullopt, 2, kStakePerNode, ackView, &ackTrackers);
    BOOST_CHECK(hasNoResends(&ackTrackers));

    updateAckTrackers(std::nullopt, 3, kStakePerNode, ackView, &ackTrackers);
    BOOST_CHECK(hasNoResends(&ackTrackers));

    updateAckTrackers(std::nullopt, 1, kStakePerNode, ackView, &ackTrackers);
    BOOST_CHECK(hasResends(&ackTrackers));

    for (uint64_t curQuack{}; curQuack < kNumTests; curQuack++)
    {
        ackView.ackOffset = curQuack + 2;
        updateAckTrackers(curQuack, 0, kStakePerNode, ackView, &ackTrackers);
        BOOST_CHECK(hasNoResends(&ackTrackers));
        for (uint64_t curNode{}; curNode < kOtherNetworkSize; curNode++)
        {
            updateAckTrackers(curQuack, curNode, kStakePerNode, ackView, &ackTrackers);
            BOOST_CHECK(hasNoResends(&ackTrackers));
        }
        updateAckTrackers(curQuack, 3, kStakePerNode, ackView, &ackTrackers);
        BOOST_CHECK(hasResends(&ackTrackers));
    }
}

BOOST_AUTO_TEST_CASE(test_single_tracker_stale_update)
{
    constexpr auto kListSize = 0;
    constexpr auto kNumTrackers = 1;
    constexpr auto kStakePerNode = 1;
    constexpr auto kNumTests = 10'000;
    auto ackTrackers = std::vector<std::unique_ptr<AcknowledgmentTracker>>();
    for (uint64_t i = 0; i < kNumTrackers; i++)
    {
        ackTrackers.push_back(std::make_unique<AcknowledgmentTracker>(kOtherNetworkSize, kOtherNetworkFailedStake));
    }

    acknowledgment::AckView<kListSize> ackView{.ackOffset = 1, .view = {}};

    for (uint64_t curQuack{}; curQuack < kNumTests; curQuack++)
    {
        ackView.ackOffset = curQuack + 2;
        for (uint64_t curNode{}; curNode < kOtherNetworkSize; curNode++)
        {
            updateAckTrackers(kNumTests, curNode, kStakePerNode, ackView, &ackTrackers);
            BOOST_CHECK(hasNoResends(&ackTrackers));
            updateAckTrackers(kNumTests, curNode, kStakePerNode, ackView, &ackTrackers);
            BOOST_CHECK(hasNoResends(&ackTrackers));
        }
    }
}

// BOOST_AUTO_TEST_CASE(test_sequential_recovery)
// {
//     const auto ackTrackers = std::make_shared<std::vector<std::unique_ptr<AcknowledgmentTracker>>>();
//     for (uint64_t i = 0; i < kNumTrackers; i++)
//     {
//         ackTrackers->push_back(std::make_unique<AcknowledgmentTracker>(
//             kOtherNetworkSize,
//             kOtherNetworkFailedStake
//         ));
//     }

// }

BOOST_AUTO_TEST_SUITE_END()

}; // namespace repeated_acknowledgment_tracker_test
