#include "scrooge.h"

#include "crypto.h"
#include "inverse_message_scheduler.h"
#include "iothread.h"
#include "proto_utils.h"
#include "scrooge_message.pb.h"
#include "scrooge_request.pb.h"
#include "scrooge_transfer.pb.h"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <fstream>
#include <limits>
#include <map>
#include <pthread.h>
#include <stdio.h>
#include <thread>
#include <unistd.h>

#include <boost/circular_buffer.hpp>
#include <boost/unordered/unordered_map.hpp>
#include <boost/unordered/unordered_set.hpp>
#include <nng/nng.h>

boost::circular_buffer<scrooge::MessageResendData> resendDatas(1 << 20);
boost::circular_buffer<acknowledgment_tracker::ResendData> requestedResends(1 << 12);

auto lastSendTime = std::chrono::steady_clock::now();
uint64_t numMsgsSentWithLastAck{};
std::optional<uint64_t> lastSentAck{};
uint64_t lastQuack = 0;
constexpr uint64_t kAckWindowSize = ACK_WINDOW;
constexpr uint64_t kQAckWindowSize = QUACK_WINDOW; // Failures maybe try (1<<20)
// Optimal window size for non-stake: 12*16 and for stake: 12*8
// Good values with ack12 (and 16), quack1000, delay1000ms
constexpr std::chrono::nanoseconds kMaxMessageDelay =
    std::chrono::duration_cast<std::chrono::nanoseconds>(MAX_MESSAGE_DELAY);
constexpr std::chrono::nanoseconds kNoopDelay = std::chrono::duration_cast<std::chrono::nanoseconds>(NOOP_DELAY);
uint64_t noop_ack = 0;
uint64_t numResendChecks{}, numActiveResends{}, numResendsOverQuack{}, numMessagesSent{}, numResendsTooHigh{},
    numResendsTooLow{}, searchDistance{}, searchChecks{};
uint64_t numQuackWindowFails{}, numAckWindowFails{}, numSendChecks{}, numIOTimeoutHits{}, numNoopTimeoutHits{},
    numTimeoutExclusiveHits{}, numResendBufFullChecks{}, outstandingResendRequests{}, numHandlResends{};
bool isResendDataUpdated{};
uint64_t maxResendRequest{};
uint64_t numMessagesResent{};

template <bool kIsUsingFile>
bool handleNewMessage(std::chrono::steady_clock::time_point curTime, const MessageScheduler &messageScheduler,
                      std::optional<uint64_t> curQuack, Pipeline *const pipeline,
                      const Acknowledgment *const acknowledgment, scrooge::CrossChainMessageData &newMessageData)
{
    const auto sequenceNumber = newMessageData.sequence_number();

    uint64_t checkVal = curQuack.value_or(0);
    lastQuack = checkVal;

    const auto resendNumber = messageScheduler.getResendNumber(sequenceNumber);
    const auto isMessageNeverSent = not resendNumber.has_value();
    if (isMessageNeverSent)
    {
        return true;
    }

    auto destinations = messageScheduler.getMessageDestinations(sequenceNumber);

    bool isFirstSender = resendNumber == 0;
    const auto isPossiblySentLater = not isFirstSender || destinations.size() > 1;
    if (isFirstSender)
    {
        const auto receiverNode = destinations.at(0);
        numMessagesSent++;

        if (isPossiblySentLater)
        {
            auto messageDataCopy = newMessageData;
            bool isMessageSent;
            if constexpr (kIsUsingFile)
            {
                isMessageSent =
                    pipeline->SendFileToOtherRsm(receiverNode, std::move(messageDataCopy), acknowledgment, curTime);
            }
            else
            {
                isMessageSent =
                    pipeline->SendToOtherRsm(receiverNode, std::move(messageDataCopy), acknowledgment, curTime);
            }
            if (isMessageSent)
            {
                lastSendTime = curTime;
                numMsgsSentWithLastAck++;
            }
        }
        else
        {
            bool isMessageSent;
            if constexpr (kIsUsingFile)
            {
                isMessageSent =
                    pipeline->SendFileToOtherRsm(receiverNode, std::move(newMessageData), acknowledgment, curTime);
            }
            else
            {
                isMessageSent =
                    pipeline->SendToOtherRsm(receiverNode, std::move(newMessageData), acknowledgment, curTime);
            }
            if (isMessageSent)
            {
                lastSendTime = curTime;
                numMsgsSentWithLastAck++;
            }
        }
    }

    if (isPossiblySentLater)
    {
        const uint64_t numDestinationsAlreadySent = isFirstSender;
        // SPDLOG_CRITICAL("ADDING A RESENDDATA S{} #{}", sequenceNumber, destinations.size());
        resendDatas.push_back(scrooge::MessageResendData{.sequenceNumber = sequenceNumber,
                                                         .firstDestinationResendNumber = resendNumber.value(),
                                                         .numDestinationsSent = numDestinationsAlreadySent,
                                                         .messageData = std::move(newMessageData),
                                                         .destinations = destinations});
        isResendDataUpdated |= sequenceNumber <= maxResendRequest;
    }
    return false;
}

void updateResendData(iothread::MessageQueue<acknowledgment_tracker::ResendData> *resendDataQueue,
                      std::optional<uint64_t> curQuack)
{
    // TODO see if this style of bulk check or a bulk erase up to std::find(first useful data) is meaningfully faster
    // constexpr auto kResendBlockSize = 4096;
    // if (resendDatas.size() > kResendBlockSize && resendDatas.at(kResendBlockSize).sequenceNumber <= curQuack)
    // {
    //     resendDatas.erase_begin(kResendBlockSize);
    // }

    while (resendDatas.size() && (resendDatas.front().sequenceNumber <= curQuack ||
                                  resendDatas.front().numDestinationsSent == resendDatas.front().destinations.size()))
    {
        // SPDLOG_CRITICAL("Removing resend data with s# {} Quack={}", resendDatas.front().sequenceNumber,
        // curQuack.value_or(0));
        resendDatas.pop_front();
    }
    const auto minimumResendSequenceNumber =
        (resendDatas.size()) ? resendDatas.front().sequenceNumber : curQuack.value_or(-1ULL) + 1;
    while (requestedResends.size() && (not requestedResends.front().isActive ||
                                       requestedResends.front().sequenceNumber < minimumResendSequenceNumber))
    {
        // SPDLOG_CRITICAL("REMOVING A REQUESTED RESEND S{}, Quack={} minS#={}",
        // requestedResends.front().sequenceNumber, curQuack.value_or(0), minimumResendSequenceNumber);
        requestedResends.pop_front();
    }

    const auto oldSize = requestedResends.size();
    acknowledgment_tracker::ResendData activeResend;
    while (not requestedResends.full() && resendDataQueue->try_dequeue(activeResend))
    {
        if (activeResend.sequenceNumber >= minimumResendSequenceNumber)
        {
            requestedResends.push_back(activeResend);
            // SPDLOG_CRITICAL("ADDING A REQUESTED RESEND S={} Quack={}", activeResend.sequenceNumber,
            // curQuack.value_or(0));
            maxResendRequest = std::max<uint64_t>(maxResendRequest, activeResend.sequenceNumber);
        }
    }
    isResendDataUpdated |= oldSize != requestedResends.size();
}

template <bool kIsUsingFile>
void handleResends(std::chrono::steady_clock::time_point curTime, Pipeline *const pipeline,
                   const Acknowledgment *const acknowledgment)
{

    auto pseudoBegin = std::begin(resendDatas);
    uint64_t prevActiveResendSequenceNum = 0;

    for (auto &requestedResend : requestedResends)
    {
        numResendChecks++;
        if (not requestedResend.isActive)
        {
            continue;
        }
        numActiveResends++;

        const bool isTooLow = requestedResend.sequenceNumber < resendDatas.front().sequenceNumber;
        if (isTooLow)
        {
            // SPDLOG_CRITICAL("RESEND TOO LOW {} {}", requestedResend.sequenceNumber,
            // resendDatas.front().sequenceNumber);
            numResendsTooLow++;
            requestedResend.isActive = false;
            continue;
        }
        numResendsOverQuack++;

        const bool isTooHigh = requestedResend.sequenceNumber > resendDatas.back().sequenceNumber;
        if (isTooHigh)
        {
            // SPDLOG_CRITICAL("RESEND TOO HIGH {} {}", requestedResend.sequenceNumber,
            // resendDatas.front().sequenceNumber);
            numResendsTooHigh++;
            break;
        }

        const auto curActiveResendSequenceNum = requestedResend.sequenceNumber;
        const bool isMonotone = prevActiveResendSequenceNum <= curActiveResendSequenceNum;
        prevActiveResendSequenceNum = curActiveResendSequenceNum;
        auto curResendDataIt = pseudoBegin;
        if (isMonotone && pseudoBegin == std::begin(resendDatas))
        {
            while (pseudoBegin + 4000 < std::end(resendDatas) &&
                   (pseudoBegin + 4000)->sequenceNumber < requestedResend.sequenceNumber)
            {
                pseudoBegin += 4000;
                searchDistance++;
            }

            while (pseudoBegin + 500 < std::end(resendDatas) &&
                   (pseudoBegin + 500)->sequenceNumber < requestedResend.sequenceNumber)
            {
                pseudoBegin += 500;
                searchDistance++;
            }

            while (pseudoBegin + 50 < std::end(resendDatas) &&
                   (pseudoBegin + 50)->sequenceNumber < requestedResend.sequenceNumber)
            {
                pseudoBegin += 50;
                searchDistance++;
            }

            curResendDataIt = std::find_if(pseudoBegin, std::end(resendDatas),
                                           [&](const scrooge::MessageResendData &possibleData) -> bool {
                                               return possibleData.sequenceNumber >= requestedResend.sequenceNumber;
                                           });
            searchDistance += std::distance(pseudoBegin, curResendDataIt);
        }
        else if (isMonotone)
        {
            curResendDataIt = std::find_if(pseudoBegin, std::end(resendDatas),
                                           [&](const scrooge::MessageResendData &possibleData) -> bool {
                                               return possibleData.sequenceNumber >= requestedResend.sequenceNumber;
                                           });

            searchDistance += std::distance(pseudoBegin, curResendDataIt);
        }
        else
        {
            // probably should do binary search?
            curResendDataIt = std::find_if(std::make_reverse_iterator(pseudoBegin), std::rend(resendDatas),
                                           [&](const scrooge::MessageResendData &possibleData) -> bool {
                                               return possibleData.sequenceNumber < requestedResend.sequenceNumber;
                                           })
                                  .base();
            searchDistance += std::distance(curResendDataIt, pseudoBegin);
        }
        searchChecks++;

        pseudoBegin = curResendDataIt;

        if (curResendDataIt == std::end(resendDatas))
        {
            SPDLOG_CRITICAL("COULDN'T FIND RESEND DATA");
            // couldn't find anything
            continue;
        }

        const bool noResendDataFound = curResendDataIt->sequenceNumber != requestedResend.sequenceNumber;
        if (noResendDataFound)
        {
            SPDLOG_CRITICAL("COULDN'T FOUND LATER DATA");
            continue;
        }

        auto &[sequenceNumber, firstDestinationResendNumber, numDestinationsSent, messageData, destinations] =
            *curResendDataIt;

        // We got to the message which should be resent
        const auto curNodeFirstResend = firstDestinationResendNumber;
        const auto curNodeLastResend = firstDestinationResendNumber + destinations.size() - 1;
        const auto curNodeCompletedResends = curNodeFirstResend + numDestinationsSent;
        const auto curFinalDestination = std::min<uint64_t>(curNodeLastResend, requestedResend.resendNumber);
        for (uint64_t resend = curNodeCompletedResends; resend <= curFinalDestination; resend++)
        {
            // SPDLOG_CRITICAL("BRO FINALLY S#{}", sequenceNumber);
            const auto destination = destinations.at(resend - curNodeFirstResend);
            const bool isSentLater = numDestinationsSent + 1 < destinations.size();
            if (isSentLater)
            {
                // SPDLOG_CRITICAL("Yo What?? S#{}", sequenceNumber);
                scrooge::CrossChainMessageData messageDataCopy;
                messageDataCopy.CopyFrom(messageData);
                if constexpr (kIsUsingFile)
                {
                    pipeline->SendFileToOtherRsm(destination, std::move(messageDataCopy), acknowledgment, curTime);
                }
                else
                {
                    pipeline->SendToOtherRsm(destination, std::move(messageDataCopy), acknowledgment, curTime);
                }
            }
            else
            {
                bool isFlushed;
                if constexpr (kIsUsingFile)
                {
                    isFlushed =
                        pipeline->SendFileToOtherRsm(destination, std::move(messageData), acknowledgment, curTime);
                }
                else
                {
                    isFlushed = pipeline->SendToOtherRsm(destination, std::move(messageData), acknowledgment, curTime);
                }
                requestedResend.isActive = false;
            }
            numDestinationsSent++;
            numMessagesResent += is_test_recording();
        }
    }
}

template <bool kIsUsingFile>
static void runScroogeSendThread(
    const std::shared_ptr<iothread::MessageQueue<scrooge::CrossChainMessageData>> messageInput,
    const std::shared_ptr<Pipeline> pipeline, const std::shared_ptr<Acknowledgment> acknowledgment,
    const std::shared_ptr<iothread::MessageQueue<acknowledgment_tracker::ResendData>> resendDataQueue,
    const std::shared_ptr<QuorumAcknowledgment> quorumAck, const NodeConfiguration configuration)
{
    SPDLOG_CRITICAL("SEND THREAD TID {}", gettid());
    const auto &[kOwnNetworkSize, kOtherNetworkSize, kOwnNetworkStakes, kOtherNetworkStakes, kOwnMaxNumFailedStake,
                 kOtherMaxNumFailedStake, kNodeId, kLogPath, kWorkingDir] = configuration;

    bindThreadToCpu(1);
    SPDLOG_INFO("Send Thread starting with TID = {}", gettid());

    const MessageScheduler messageScheduler(configuration);
    uint64_t peekSN = 0;
    const double max_txn_per_s = 800'000.0;
    const auto test_start_time = std::chrono::steady_clock::now();

    while (not is_test_over())
    {
        // update window information
        const auto curAck = acknowledgment->getAckIterator();
        const auto curQuack = quorumAck->getCurrentQuack();

        if (curAck != lastSentAck)
        {
            lastSentAck = curAck;
            numMsgsSentWithLastAck = 0;
        }

        int64_t pendingSequenceNum = peekSN;
        if constexpr (not kIsUsingFile)
        {
            pendingSequenceNum = pendingSequenceNum = (messageInput->peek()) ? messageInput->peek()->sequence_number()
                                                                             : kQAckWindowSize + curQuack.value_or(0);
        }
        else
        {
            const auto cur_time = std::chrono::steady_clock::now();
            const auto test_duration_seconds = std::chrono::duration<double>(cur_time - test_start_time);
            const auto cur_throughput = ((int64_t)pendingSequenceNum - 5000) / test_duration_seconds.count();
            if (cur_throughput > max_txn_per_s)
            {
                pendingSequenceNum = kQAckWindowSize + curQuack.value_or(0);
            }
        }
        const bool isAckFresh = numMsgsSentWithLastAck < kAckWindowSize;
        const bool isSequenceNumberUseful = pendingSequenceNum - curQuack.value_or(0ULL - 1) < kQAckWindowSize;
        const auto curTime = std::chrono::steady_clock::now();
        const bool isNoopTimeoutHit = curTime - lastSendTime > kNoopDelay;
        const bool isTimeoutHit = curTime - lastSendTime > kMaxMessageDelay;
        const bool shouldDequeue = isTimeoutHit || (isAckFresh && isSequenceNumberUseful);

        numSendChecks++;
        numQuackWindowFails += not isSequenceNumberUseful;
        numAckWindowFails += not isAckFresh;
        numIOTimeoutHits += isTimeoutHit;
        numTimeoutExclusiveHits += isTimeoutHit && not(isSequenceNumberUseful && isAckFresh);
        numNoopTimeoutHits += isNoopTimeoutHit;

        scrooge::CrossChainMessageData newMessageData;
        bool shouldHandleNewMessage;
        if constexpr (kIsUsingFile)
        {
            shouldHandleNewMessage = not resendDatas.full() && shouldDequeue;
            numResendBufFullChecks += resendDatas.full();
        }
        else
        {
            shouldHandleNewMessage = not resendDatas.full() && shouldDequeue &&
                                     messageInput->try_dequeue(newMessageData) && not is_test_over();
        }
        if (shouldHandleNewMessage)
        {
            if constexpr (kIsUsingFile)
            {
                newMessageData = util::getNextMessage();
            }
            peekSN++;
            handleNewMessage<kIsUsingFile>(curTime, messageScheduler, curQuack, pipeline.get(), acknowledgment.get(),
                                           newMessageData);
            // if (shouldContinue)
            // {
            //     continue;
            // }
        }
        else if (isAckFresh && isNoopTimeoutHit)
        {
            static uint64_t receiver = 0;

            if constexpr (kIsUsingFile)
            {
                pipeline->forceSendFileToOtherRsm(receiver % kOtherNetworkSize, acknowledgment.get(), curTime);
            }
            else
            {
                pipeline->forceSendToOtherRsm(receiver % kOtherNetworkSize, acknowledgment.get(), curTime);
            }

            receiver++;
            numMsgsSentWithLastAck++;
            noop_ack++;

            lastSendTime = curTime;
        }

        updateResendData(resendDataQueue.get(), curQuack);

        if (not isResendDataUpdated || resendDatas.empty() || requestedResends.empty())
        {
            continue;
        }

        outstandingResendRequests += requestedResends.size();
        numHandlResends++;
        handleResends<kIsUsingFile>(curTime, pipeline.get(), acknowledgment.get());

        isResendDataUpdated = false;
    }

    addMetric("Noop Acks", noop_ack);
    addMetric("Noop Delay", std::chrono::duration<double>(kNoopDelay).count());
    addMetric("Max message delay", std::chrono::duration<double>(kMaxMessageDelay).count());
    addMetric("Ack Window", kAckWindowSize);
    addMetric("Quack Window", kQAckWindowSize);
    addMetric("transfer_strategy", "Scrooge double-klist"s + " k=" + std::to_string(kListSize) + " noop[" +
                                       std::to_string(std::chrono::duration<double>(kNoopDelay).count() * 1000) +
                                       "ms] MMDelay[" +
                                       std::to_string(std::chrono::duration<double>(kMaxMessageDelay).count() * 1000) +
                                       "ms]" + " quackWindow[" + std::to_string(kQAckWindowSize) + "] ackWindow[" +
                                       std::to_string(kAckWindowSize) + "]");
    addMetric("num_msgs_sent_primary", numMessagesSent);
    addMetric("num_msgs_resent", numMessagesResent);
    addMetric("num_resend_checks", numResendChecks);
    addMetric("avg_resend_active", (double)numActiveResends / numResendChecks);
    addMetric("avg_resend_active_over_quack", (double)numResendsOverQuack / numActiveResends);
    addMetric("avg_resend_too_low", (double)numResendsTooLow / numActiveResends);
    addMetric("avg_resend_too_high", (double)numResendsTooHigh / numActiveResends);
    addMetric("avg_search_dist", (double)searchDistance / searchChecks);
    addMetric("Quack-Fail", (double)numQuackWindowFails / numSendChecks);
    addMetric("Ack-Fail", (double)numAckWindowFails / numSendChecks);
    addMetric("Timeout-Hits", (double)numIOTimeoutHits / numSendChecks);
    addMetric("Noop-Hits", (double)numNoopTimeoutHits / numSendChecks);
    addMetric("Full-ResendBuf%", (double)numResendBufFullChecks / numSendChecks);
    addMetric("OutstandingResends", (double)outstandingResendRequests / numHandlResends);
    addMetric("Timeout-Exclusive-Hits", (double)numTimeoutExclusiveHits / numSendChecks);

    SPDLOG_INFO("ALL CROSS CONSENSUS PACKETS SENT : send thread exiting");
}

void runScroogeSendThread(std::shared_ptr<iothread::MessageQueue<scrooge::CrossChainMessageData>> messageInput,
                          std::shared_ptr<Pipeline> pipeline, std::shared_ptr<Acknowledgment> acknowledgment,
                          std::shared_ptr<iothread::MessageQueue<acknowledgment_tracker::ResendData>> resendDataQueue,
                          std::shared_ptr<QuorumAcknowledgment> quorumAck, NodeConfiguration configuration)
{
    constexpr bool kIsUsingFile = false;
    runScroogeSendThread<kIsUsingFile>(messageInput, pipeline, acknowledgment, resendDataQueue, quorumAck,
                                       configuration);
}

void runFileScroogeSendThread(
    std::shared_ptr<iothread::MessageQueue<scrooge::CrossChainMessageData>> messageInput,
    std::shared_ptr<Pipeline> pipeline, std::shared_ptr<Acknowledgment> acknowledgment,
    std::shared_ptr<iothread::MessageQueue<acknowledgment_tracker::ResendData>> resendDataQueue,
    std::shared_ptr<QuorumAcknowledgment> quorumAck, NodeConfiguration configuration)
{
    constexpr bool kIsUsingFile = true;
    runScroogeSendThread<kIsUsingFile>(messageInput, pipeline, acknowledgment, resendDataQueue, quorumAck,
                                       configuration);
}

uint64_t numMissing = 0;
uint64_t numChecked = 0;
uint64_t numRecv = 0;
uint64_t numRequests = 0;
int64_t klistDelta = 0;
int64_t overlap = 0;

struct LameTracker
{
    uint64_t sequenceNumber;
    uint64_t firstResendNum;
    uint64_t lastResendNum;
    AcknowledgmentTracker ackTracker;
};
void updateForeignAckTrackers(const std::optional<uint64_t> curQuack, const uint64_t nodeStake,
                              const util::JankAckView nodeAckView, std::vector<LameTracker> &ackTrackers,
                              iothread::MessageQueue<acknowledgment_tracker::ResendData> *const resendDataQueue,
                              const MessageScheduler &messageScheduler)
{
    numRecv++;
    const auto nodeId = nodeAckView.senderId;
    const auto kNumAckTrackers = ackTrackers.size();
    const auto initialMessageTrack = curQuack.value_or(0ULL - 1ULL) + 1;
    const auto finialMessageTrack = std::min(initialMessageTrack + kNumAckTrackers - 1, util::getFinalAck(nodeAckView));
    overlap += (finialMessageTrack >= initialMessageTrack)
                   ? ((int64_t)finialMessageTrack - (int64_t)initialMessageTrack + 1)
                   : 0;
    klistDelta += (int64_t)initialMessageTrack - (int64_t)util::getAckIterator(nodeAckView).value_or(0);

    // SPDLOG_CRITICAL("MAKING UPDATE FOR NODE {} -- Q{} Init{} Final{}", nodeId, curQuack.value_or(0),
    // initialMessageTrack, finialMessageTrack);
    for (uint64_t curMessage = initialMessageTrack; curMessage <= finialMessageTrack; curMessage++)
    {
        const auto curAckTracker = ackTrackers.data() + (curMessage % ackTrackers.size());
        if (curAckTracker->sequenceNumber != curMessage)
        {
            const auto firstResendNumber = messageScheduler.getResendNumber(curMessage);
            if (firstResendNumber < 1)
            {
                curAckTracker->sequenceNumber = curMessage;
                curAckTracker->lastResendNum = 0;
                continue;
            }
            const auto lastResendNumber =
                *firstResendNumber + messageScheduler.getMessageDestinations(curMessage).size() - 1;
            curAckTracker->sequenceNumber = curMessage;
            curAckTracker->firstResendNum = *firstResendNumber;
            curAckTracker->lastResendNum = lastResendNumber;
        }

        const bool isNeverResent = curAckTracker->lastResendNum == 0;
        if (isNeverResent)
        {
            continue;
        }

        numChecked++;
        const auto virtualQuack = (curMessage) ? std::optional<uint64_t>(curMessage - 1) : std::nullopt;
        const auto isNodeMissingCurMessage = not util::testAckView(nodeAckView, curMessage);

        if (isNodeMissingCurMessage)
        {
            numMissing++;
            const auto update = curAckTracker->ackTracker.update(nodeId, nodeStake, virtualQuack, virtualQuack);
            // if (update.isActive)
            // {
            //     SPDLOG_CRITICAL("Active Update N{}: [{} {}], S{}, #[{}<->{}]", nodeId, update.sequenceNumber,
            //     update.resendNumber, curMessage, curAckTracker->firstResendNum, curAckTracker->lastResendNum);
            // }
            // else
            // {
            //     SPDLOG_CRITICAL("NonActive Update N{}: S{}, #[{}<->{}]", nodeId, curMessage,
            //     curAckTracker->firstResendNum, curAckTracker->lastResendNum);
            // }

            const bool isUpdateUseful = update.isActive && curAckTracker->firstResendNum <= update.resendNumber &&
                                        update.resendNumber <= curAckTracker->lastResendNum;
            if (isUpdateUseful)
            {
                while (not resendDataQueue->try_enqueue(update) && not is_test_over())
                    ;
                numRequests++;
            }
        }
        else
        {
            curAckTracker->ackTracker.update(nodeId, nodeStake, virtualQuack.value_or(0ULL - 1ULL) + 1, virtualQuack);
            // if (update.isActive)
            // {
            //     SPDLOG_CRITICAL("Active Update N{}: [{} {}], S{}, #[{}<->{}]", nodeId, update.sequenceNumber,
            //     update.resendNumber, curMessage, curAckTracker->firstResendNum, curAckTracker->lastResendNum);
            // }
            // else
            // {
            //     SPDLOG_CRITICAL("NonActive- Update N{}: S{}, #[{}<->{}]", nodeId, curMessage,
            //     curAckTracker->firstResendNum, curAckTracker->lastResendNum);
            // }
        }
    }
}

uint64_t numMissingLocal = 0;
uint64_t numCheckedLocal = 0;
uint64_t numRecvLocal = 0;
uint64_t numRequestsLocal = 0;
int64_t klistDeltaLocal = 0;
int64_t overlapLocal = 0;
int64_t kListSizeLocal = 0;

struct LameLocalTracker
{
    uint64_t sequenceNumber;
    uint64_t minResendNumber;
    AcknowledgmentTracker ackTracker;
};
void updateLocalAckTrackers(const std::optional<uint64_t> curQuack, const uint64_t nodeStake,
                            const util::JankAckView nodeAckView, std::vector<LameLocalTracker> &ackTrackers,
                            iothread::MessageQueue<uint64_t> *const rebroadcastDataQueue,
                            const InverseMessageScheduler &inverseMessageScheduler)
{
    numRecvLocal++;
    const auto nodeId = nodeAckView.senderId;
    const auto kNumAckTrackers = ackTrackers.size();
    const auto initialMessageTrack = curQuack.value_or(0ULL - 1ULL) + 1;
    const auto finialMessageTrack = std::min(initialMessageTrack + kNumAckTrackers - 1, util::getFinalAck(nodeAckView));
    overlapLocal += (finialMessageTrack >= initialMessageTrack)
                        ? ((int64_t)finialMessageTrack - (int64_t)initialMessageTrack + 1)
                        : 0;
    klistDeltaLocal += (int64_t)initialMessageTrack - (int64_t)util::getAckIterator(nodeAckView).value_or(0);
    kListSizeLocal += (int64_t)finialMessageTrack - (int64_t)initialMessageTrack + 1;

    // SPDLOG_CRITICAL("MAKING UPDATE FOR NODE {} -- Q{} Init{} Final{}", nodeId, curQuack.value_or(0),
    // initialMessageTrack, finialMessageTrack);
    for (uint64_t curMessage = initialMessageTrack; curMessage <= finialMessageTrack; curMessage++)
    {
        const auto curAckTracker = ackTrackers.data() + (curMessage % ackTrackers.size());
        if (curAckTracker->sequenceNumber != curMessage)
        {
            const auto firstResendNumber = inverseMessageScheduler.getMinResendNumber(curMessage);
            curAckTracker->sequenceNumber = curMessage;
            curAckTracker->minResendNumber = firstResendNumber.value_or(0);
        }

        const bool isNeverResent = curAckTracker->minResendNumber == 0;
        if (isNeverResent)
        {
            continue;
        }

        numCheckedLocal++;
        const auto virtualQuack = (curMessage) ? std::optional<uint64_t>(curMessage - 1) : std::nullopt;
        const auto isNodeMissingCurMessage = not util::testAckView(nodeAckView, curMessage);

        if (isNodeMissingCurMessage)
        {
            numMissingLocal++;
            const auto update = curAckTracker->ackTracker.update(nodeId, nodeStake, virtualQuack, virtualQuack);
            // if (update.isActive)
            // {
            //     SPDLOG_CRITICAL("Active Update N{}: [{} {}], S{}, #[{}<->{}]", nodeId, update.sequenceNumber,
            //     update.resendNumber, curMessage, curAckTracker->firstResendNum, curAckTracker->lastResendNum);
            // }
            // else
            // {
            //     SPDLOG_CRITICAL("NonActive Update N{}: S{}, #[{}<->{}]", nodeId, curMessage,
            //     curAckTracker->firstResendNum, curAckTracker->lastResendNum);
            // }

            const bool isUpdateUseful = update.isActive && curAckTracker->minResendNumber <= update.resendNumber;
            if (isUpdateUseful)
            {
                while (not rebroadcastDataQueue->try_enqueue(curMessage) && not is_test_over())
                    ;
                numRequests++;
                curAckTracker->minResendNumber = 0; // ignore this message in the future
            }
        }
        else
        {
            curAckTracker->ackTracker.update(nodeId, nodeStake, virtualQuack.value_or(0ULL - 1ULL) + 1, virtualQuack);
            // if (update.isActive)
            // {
            //     SPDLOG_CRITICAL("Active Update N{}: [{} {}], S{}, #[{}<->{}]", nodeId, update.sequenceNumber,
            //     update.resendNumber, curMessage, curAckTracker->firstResendNum, curAckTracker->lastResendNum);
            // }
            // else
            // {
            //     SPDLOG_CRITICAL("NonActive- Update N{}: S{}, #[{}<->{}]", nodeId, curMessage,
            //     curAckTracker->firstResendNum, curAckTracker->lastResendNum);
            // }
        }
    }
}

void lameAckThread(Acknowledgment *const acknowledgment, QuorumAcknowledgment *const quorumAck,
                   QuorumAcknowledgment *const localQuack,
                   iothread::MessageQueue<acknowledgment_tracker::ResendData> *const resendDataQueue,
                   iothread::MessageQueue<uint64_t> *const rebroadcastDataQueue,
                   moodycamel::ReaderWriterQueue<util::JankAckView> *const viewQueue, NodeConfiguration configuration)
{
    bindThreadToCpu(3);
    SPDLOG_CRITICAL("STARTING LAME THREAD {}", gettid());
    MessageScheduler messageScheduler{configuration};
    InverseMessageScheduler inverseMessageScheduler{configuration};

    constexpr auto kNumAckTrackers = 8192;
    std::vector<LameTracker> foreignAckTrackers;
    std::vector<LameLocalTracker> localAckTrackers;
    for (uint64_t i = 0; i < kNumAckTrackers; i++)
    {
        foreignAckTrackers.push_back(
            LameTracker{.sequenceNumber = -1ULL,
                        .firstResendNum = -1ULL,
                        .lastResendNum = -1ULL,
                        .ackTracker = AcknowledgmentTracker{configuration.kOtherNetworkSize,
                                                            configuration.kOtherMaxNumFailedStake}});
        localAckTrackers.push_back(LameLocalTracker{
            .sequenceNumber = -1ULL,
            .minResendNumber = -1ULL,
            .ackTracker = AcknowledgmentTracker{configuration.kOwnNetworkSize, configuration.kOwnMaxNumFailedStake}});
    }

    while (not is_test_over())
    {
        util::JankAckView curView{};
        while (not viewQueue->try_dequeue(curView) && not is_test_over())
            ;

        if (curView.isLocal)
        {
            const auto senderStake = configuration.kOwnNetworkStakes.at(curView.senderId);
            const auto currentQuack = localQuack->getCurrentQuack();
            updateLocalAckTrackers(currentQuack, senderStake, curView, localAckTrackers, rebroadcastDataQueue,
                                   inverseMessageScheduler);
        }
        else
        {
            const auto senderStake = configuration.kOtherNetworkStakes.at(curView.senderId);
            const auto currentQuack = quorumAck->getCurrentQuack();
            updateForeignAckTrackers(currentQuack, senderStake, curView, foreignAckTrackers, resendDataQueue,
                                     messageScheduler);
        }
    }
    SPDLOG_CRITICAL("LAME THREAD ENDING");

    uint64_t untochedLocal{}, untochedForeign{};
    util::JankAckView curView{};

    while (viewQueue->try_dequeue(curView))
    {
        untochedLocal += curView.isLocal;
        untochedForeign += curView.isLocal ^ 1;
    }

    addMetric("ResendRequests", numRequests);
    addMetric("klistDelta", (double)klistDelta / numRecv);
    addMetric("overlap", (double)overlap / numRecv);
    addMetric("untouched klists", untochedForeign);
    addMetric("ResendRequestsLocal", numRequestsLocal);
    addMetric("klistDeltaLocal", (double)klistDeltaLocal / numRecvLocal);
    addMetric("overlapLocal", (double)overlapLocal / numRecvLocal);
    addMetric("untouched klists Local", untochedLocal);
    addMetric("Avg Klist size local", (double)kListSizeLocal / numRecvLocal);
}

void runScroogeReceiveThread(
    const std::shared_ptr<Pipeline> pipeline, const std::shared_ptr<Acknowledgment> acknowledgment,
    const std::shared_ptr<iothread::MessageQueue<acknowledgment_tracker::ResendData>> resendDataQueue,
    const std::shared_ptr<QuorumAcknowledgment> quorumAck, const NodeConfiguration configuration)
{
    bindThreadToCpu(2);
    SPDLOG_CRITICAL("RECV THREAD TID {}", gettid());

    uint64_t timedMessages{}, needlessRebroadcasts{}, numMsgsFromOtherRsm{}, numMsgsFromOwnRsm{};
    pipeline::ReceivedCrossChainMessage receivedMessage{};

    scrooge::CrossChainMessage crossChainMessage;

    moodycamel::ReaderWriterQueue<util::JankAckView> viewQueue(1 << 12);
    iothread::MessageQueue<uint64_t> rebroadcastDataQueue(1 << 12);
    InverseMessageScheduler inverseMessageScheduler{configuration};
    boost::unordered_set<uint64_t> rebroadcastRequests;
    boost::unordered_map<uint64_t, scrooge::CrossChainMessageData> rebroadcastDataBuffer; // Please make me faster
    boost::circular_buffer<scrooge::CrossChainMessageData> rebraodcastRequestBuffer(1 << 15);
    // boost::circular_buffer<scrooge::CrossChainMessageData> rebraodcastDataBuffer(1<<18);
    QuorumAcknowledgment localQuack(configuration.kOwnMaxNumFailedStake + 1);
    const auto kOwnNodeStake = configuration.kOwnNetworkStakes.at(configuration.kNodeId);
    std::thread lameThread(lameAckThread, acknowledgment.get(), quorumAck.get(), &localQuack, resendDataQueue.get(),
                           &rebroadcastDataQueue, &viewQueue, configuration);

    uint64_t lastRebroadcastGc{};
    while (not is_test_over())
    {
        if (receivedMessage.message == nullptr)
        {
            receivedMessage = pipeline->RecvFromOtherRsm();

            if (receivedMessage.message != nullptr)
            {
                auto &[message, senderId] = receivedMessage;

                const auto messageData = nng_msg_body(message);
                const auto messageSize = nng_msg_len(message);
                bool success = crossChainMessage.ParseFromArray(messageData, messageSize);
                if (not success)
                {
                    SPDLOG_CRITICAL("Cannot parse foreign message");
                    nng_msg_free(message);
                    continue;
                }

                bool isMessageValid = util::checkMessageMac(crossChainMessage);
                if (not isMessageValid)
                {
                    SPDLOG_CRITICAL("Received invalid mac");
                    nng_msg_free(receivedMessage.message);
                    receivedMessage.message = nullptr;
                    receivedMessage.senderId = 0;
                    continue;
                }

                const auto curLocalQuack = localQuack.getCurrentQuack();
                for (uint64_t curMessageInd = 0; curMessageInd < crossChainMessage.data_size(); curMessageInd++)
                {
                    const auto &messageData = crossChainMessage.data().at(curMessageInd);
                    if (not util::isMessageDataValid(messageData))
                    {
                        continue;
                    }
                    const auto curSequenceNumber = messageData.sequence_number();
                    acknowledgment->addToAckList(curSequenceNumber);

                    const auto resendNumber = inverseMessageScheduler.getMinResendNumber(curSequenceNumber);
                    // assert(resendNumber.has_value() && "Should fix this");
                    const bool isMessageRebroadcast = resendNumber > 0;
                    const bool isMessageDelivered = curLocalQuack > curSequenceNumber;
                    if (isMessageDelivered)
                    {
                        // Delete the message because it is already delivered -- micro-op which can be attacked by byz
                        // nodes std::swap(crossChainMessage.mutable_data()->at(curMessageInd),
                        // crossChainMessage.mutable_data()->at(crossChainMessage.data_size()-1));
                        // crossChainMessage.mutable_data()->RemoveLast();
                        needlessRebroadcasts++;
                    }
                    if (isMessageRebroadcast)
                    {
                        const auto possibleRequest = rebroadcastRequests.find(curSequenceNumber);
                        if (possibleRequest != rebroadcastRequests.end())
                        {
                            // we've been looking for this! by doing nothing it'll get rebroadcast
                            rebroadcastRequests.erase(possibleRequest);
                        }
                        else
                        {
                            // Don't broadcast just yet..
                            rebroadcastDataBuffer.emplace(
                                curSequenceNumber, std::move(crossChainMessage.mutable_data()->at(curMessageInd)));
                            crossChainMessage.mutable_data()->at(curMessageInd) =
                                std::move(crossChainMessage.mutable_data()->at(crossChainMessage.data_size() - 1));
                            crossChainMessage.mutable_data()->RemoveLast();
                            curMessageInd--;
                        }
                    }

                    timedMessages += is_test_recording();
                    numMsgsFromOtherRsm++;
                }
                const auto newLocalQuack = localQuack.updateNodeAck(configuration.kNodeId, kOwnNodeStake,
                                                                    acknowledgment->getAckIterator().value_or(0));

                const auto curForeignAck = (crossChainMessage.has_ack_count())
                                               ? std::optional<uint64_t>(crossChainMessage.ack_count().value())
                                               : std::nullopt;
                const auto senderStake = configuration.kOtherNetworkStakes.at(senderId);
                if (curForeignAck.has_value())
                {
                    quorumAck->updateNodeAck(senderId, senderStake, *curForeignAck);
                }
                while (not viewQueue.try_enqueue(
                           util::JankAckView{.isLocal = false,
                                             .senderId = (uint8_t)senderId,
                                             .ackOffset = (uint32_t)(curForeignAck.value_or(0ULL - 1ULL) + 2),
                                             .view = std::move(*crossChainMessage.mutable_ack_set())}) &&
                       not is_test_over())
                    ;
                const auto curAckView = acknowledgment->getAckView<(kListSize)>();
                const auto ackIterator =
                    std::max(newLocalQuack,
                             acknowledgment::getAckIterator(curAckView)); // also micro-op maybe attackable by byz nodes
                if (ackIterator.has_value())
                {
                    // if (configuration.kNodeId % 3 == 1)
                    // {
                    //     crossChainMessage.mutable_ack_count()->set_value(999'999'999);
                    // }
                    // else
                    // {
                    //     crossChainMessage.mutable_ack_count()->set_value(ackIterator.value());
                    // }
                    crossChainMessage.mutable_ack_count()->set_value(ackIterator.value());
                }
                *crossChainMessage.mutable_ack_set() = {curAckView.view.begin(),
                                                        std::find(curAckView.view.begin(), curAckView.view.end(), 0)};

                uint64_t curRebroadcastRequest{};
                while (rebroadcastDataQueue.try_dequeue(curRebroadcastRequest))
                {
                    const auto possibleRebroadcastMessage = rebroadcastDataBuffer.find(curRebroadcastRequest);
                    if (possibleRebroadcastMessage != rebroadcastDataBuffer.end())
                    {
                        crossChainMessage.mutable_data()->Add(std::move(possibleRebroadcastMessage->second));
                        rebroadcastDataBuffer.erase(possibleRebroadcastMessage);
                    }
                    else
                    {
                        rebroadcastRequests.insert(curRebroadcastRequest);
                    }
                }

                const auto mstr = crossChainMessage.ack_count().SerializeAsString();
                // Sign with own key.
                std::string encoded = CmacSignString(get_priv_key(), mstr);
                crossChainMessage.set_validity_proof(encoded);

                const auto protoSize = crossChainMessage.ByteSizeLong();
                if (protoSize <= messageSize)
                {
                    const auto sizeShrink = messageSize - protoSize;
                    nng_msg_chop(message, sizeShrink);
                }
                else
                {
                    nng_msg_free(message);
                    nng_msg_alloc(
                        &message,
                        protoSize); // extending the msg may copy a bunch of unneeded data. Just make a new one
                }
                crossChainMessage.SerializeToArray(nng_msg_body(message), protoSize);
            }
        }

        if (receivedMessage.message)
        {
            if (crossChainMessage.data_size() == 0)
            {
                // no useful data to rebroadcast
                nng_msg_free(receivedMessage.message);
                receivedMessage.message = nullptr;
            }
            else
            {
                bool success = pipeline->rebroadcastToOwnRsm(receivedMessage.message);
                if (success)
                {
                    receivedMessage.message = nullptr;
                }
            }
        }

        const auto [message, senderId] = pipeline->RecvFromOwnRsm();

        if (message)
        {
            const auto messageData = nng_msg_body(message);
            const auto messageSize = nng_msg_len(message);
            bool success = crossChainMessage.ParseFromArray(messageData, messageSize);

            nng_msg_free(message);
            if (not success)
            {
                SPDLOG_CRITICAL("Cannot parse local message");
                receivedMessage.message = nullptr;
                receivedMessage.senderId = 0;
                continue;
            }

            const auto curForeignAck = (crossChainMessage.has_ack_count())
                                           ? std::optional<uint64_t>(crossChainMessage.ack_count().value())
                                           : std::nullopt;

            bool isMessageValid = util::checkMessageMac(crossChainMessage);
            if (not isMessageValid)
            {
                SPDLOG_CRITICAL("Received invalid mac");
                receivedMessage.message = nullptr;
                receivedMessage.senderId = 0;
                continue;
            }
            const auto senderStake = configuration.kOwnNetworkStakes.at(senderId);
            if (curForeignAck.has_value())
            {
                localQuack.updateNodeAck(senderId, senderStake, *curForeignAck);
            }
            while (not viewQueue.try_enqueue(
                       util::JankAckView{.isLocal = true,
                                         .senderId = (uint8_t)senderId,
                                         .ackOffset = (uint32_t)(curForeignAck.value_or(0ULL - 1ULL) + 2),
                                         .view = std::move(*crossChainMessage.mutable_ack_set())}) &&
                   not is_test_over())
                ;

            for (const auto &messageData : crossChainMessage.data())
            {
                if (not util::isMessageDataValid(messageData))
                {
                    continue;
                }
                acknowledgment->addToAckList(messageData.sequence_number());
                timedMessages += is_test_recording();
                numMsgsFromOwnRsm++;
            }
            const auto curQuack = localQuack.updateNodeAck(configuration.kNodeId, kOwnNodeStake,
                                                           acknowledgment->getAckIterator().value_or(0));

            if ((int64_t)curQuack.value_or(0) - (int64_t)lastRebroadcastGc > 1'000'000)
            {
                for (auto curReq = rebroadcastRequests.begin(); curReq != rebroadcastRequests.end();)
                {
                    if (*curReq < curQuack)
                    {
                        curReq = rebroadcastRequests.erase(curReq);
                    }
                    else
                    {
                        curReq++;
                    }
                }

                for (auto curMsg = rebroadcastDataBuffer.begin(); curMsg != rebroadcastDataBuffer.end();)
                {
                    if (curMsg->first < curQuack)
                    {
                        curMsg = rebroadcastDataBuffer.erase(curMsg);
                    }
                    else
                    {
                        curMsg++;
                    }
                }
                lastRebroadcastGc = curQuack.value_or(0);
            }
        }
    }

    if (receivedMessage.message)
    {
        nng_msg_free(receivedMessage.message);
    }

    lameThread.join();

    SPDLOG_INFO("ALL MESSAGES RECEIVED : Receive thread exiting");
    addMetric("Average Missing Acks", (double)numMissing / (double)numChecked);
    addMetric("Average KList Size", (double)numChecked / (double)numRecv);
    addMetric("NumKListRecv", numRecv);
    addMetric("timed_messages", timedMessages);
    addMetric("foreign_messages_received_from_other_rsm", numMsgsFromOtherRsm);
    addMetric("foreign_messages_received_from_own_rsm", numMsgsFromOwnRsm);
    addMetric("max_acknowledgment", acknowledgment->getAckIterator().value_or(0));
    addMetric("max_quorum_acknowledgment", quorumAck->getCurrentQuack().value_or(0));
    addMetric("num_needless_rebroadcasts", needlessRebroadcasts);
}
