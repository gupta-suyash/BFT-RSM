#include "scrooge.h"

#include "crypto.h"
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
#include <nng/nng.h>

boost::circular_buffer<scrooge::MessageResendData> resendDatas(1 << 20);
boost::circular_buffer<acknowledgment_tracker::ResendData> requestedResends(1 << 12);

auto lastSendTime = std::chrono::steady_clock::now();
uint64_t numMsgsSentWithLastAck{};
std::optional<uint64_t> lastSentAck{};
uint64_t lastQuack = 0;
constexpr uint64_t kAckWindowSize = 1000;
constexpr uint64_t kQAckWindowSize = 1000000000; // Failures maybe try (1<<20)
// Optimal window size for non-stake: 12*16 and for stake: 12*8
// Good values with ack12 (and 16), quack1000, delay1000ms
constexpr auto kMaxMessageDelay = 500ms;
constexpr auto kNoopDelay = 5ms;
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

    while (resendDatas.size() && (resendDatas.front().sequenceNumber <= curQuack || resendDatas.front().numDestinationsSent == resendDatas.front().destinations.size()))
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
            handleNewMessage<kIsUsingFile>(
                curTime, messageScheduler, curQuack, pipeline.get(), acknowledgment.get(), newMessageData);
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
    addMetric("Ack Window", kAckWindowSize);
    addMetric("Quack Window", kQAckWindowSize);
    addMetric("transfer_strategy", "Scrooge k=" + std::to_string(kListSize) + "+ resends");
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
    addMetric("Full-ResendBuf%", (double) numResendBufFullChecks / numSendChecks);
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
void updateAckTrackers(const std::optional<uint64_t> curQuack, const uint64_t nodeStake,
                       const util::JankAckView nodeAckView, std::vector<LameTracker> &ackTrackers,
                       iothread::MessageQueue<acknowledgment_tracker::ResendData> *const resendDataQueue,
                       const MessageScheduler &messageScheduler)
{
    numRecv++;
    const auto nodeId = nodeAckView.senderId;
    const auto kNumAckTrackers = ackTrackers.size();
    const auto initialMessageTrack = curQuack.value_or(0ULL - 1ULL) + 1;
    const auto finialMessageTrack = std::min(initialMessageTrack + kNumAckTrackers - 1, util::getFinalAck(nodeAckView));
    overlap += (finialMessageTrack >= initialMessageTrack)? ((int64_t)finialMessageTrack - (int64_t)initialMessageTrack + 1) : 0;
    klistDelta += (int64_t) initialMessageTrack - (int64_t) util::getAckIterator(nodeAckView).value_or(0);

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

void lameAckThread(Acknowledgment *const acknowledgment, QuorumAcknowledgment *const quorumAck,
                   iothread::MessageQueue<acknowledgment_tracker::ResendData> *const resendDataQueue,
                   moodycamel::ReaderWriterQueue<util::JankAckView> *const viewQueue, NodeConfiguration configuration)
{
    bindThreadToCpu(3);
    SPDLOG_CRITICAL("STARTING LAME THREAD {}", gettid());
    MessageScheduler messageScheduler{configuration};

    constexpr auto kNumAckTrackers = 8192;
    std::vector<LameTracker> ackTrackers;
    for (uint64_t i = 0; i < kNumAckTrackers; i++)
    {
        ackTrackers.push_back(LameTracker{.sequenceNumber = -1ULL,
                                          .firstResendNum = -1ULL,
                                          .lastResendNum = -1ULL,
                                          .ackTracker = AcknowledgmentTracker{configuration.kOtherNetworkSize,
                                                                              configuration.kOtherMaxNumFailedStake}});
    }

    while (not is_test_over())
    {
        util::JankAckView curView{};
        while (not viewQueue->try_dequeue(curView) && not is_test_over())
            ;
        
        // while(viewQueue->try_dequeue(curView)); // try to throw all away

        // if (is_test_over())
        // {
        //     return;
        // } // commenting this out is a bug but doesn't really matter since test is over

        const auto senderStake = configuration.kOtherNetworkStakes.at(curView.senderId);
        const auto currentQuack = quorumAck->getCurrentQuack();
        updateAckTrackers(currentQuack, senderStake, curView, ackTrackers, resendDataQueue, messageScheduler);
    }
    SPDLOG_CRITICAL("LAME THREAD ENDING");

    addMetric("ResendRequests", numRequests);
    addMetric("klistDelta", (double)klistDelta / numRecv);
    addMetric("overlap", (double)overlap / numRecv);
    addMetric("untouched klists", viewQueue->size_approx());
}

void runScroogeReceiveThread(
    const std::shared_ptr<Pipeline> pipeline, const std::shared_ptr<Acknowledgment> acknowledgment,
    const std::shared_ptr<iothread::MessageQueue<acknowledgment_tracker::ResendData>> resendDataQueue,
    const std::shared_ptr<QuorumAcknowledgment> quorumAck, const NodeConfiguration configuration)
{
    bindThreadToCpu(2);
    SPDLOG_CRITICAL("RECV THREAD TID {}", gettid());

    uint64_t timedMessages{};
    pipeline::ReceivedCrossChainMessage receivedMessage{};

    scrooge::CrossChainMessage crossChainMessage;

    moodycamel::ReaderWriterQueue<util::JankAckView> viewQueue(1 << 12);
    std::thread lameThread(lameAckThread, acknowledgment.get(), quorumAck.get(), resendDataQueue.get(), &viewQueue,
                           configuration);

    while (not is_test_over())
    {
        if (receivedMessage.message == nullptr)
        {
            receivedMessage = pipeline->RecvFromOtherRsm();

            if (receivedMessage.message != nullptr)
            {
                const auto [message, senderId] = receivedMessage;

                const auto messageData = nng_msg_body(message);
                const auto messageSize = nng_msg_len(message);
                bool success = crossChainMessage.ParseFromArray(messageData, messageSize);
                if (not success)
                {
                    SPDLOG_CRITICAL("Cannot parse foreign message");
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

                for (const auto &messageData : crossChainMessage.data())
                {
                    if (not util::isMessageDataValid(messageData))
                    {
                        continue;
                    }
                    acknowledgment->addToAckList(messageData.sequence_number());
                    timedMessages += is_test_recording();
                }

                if (crossChainMessage.data_size() == 0)
                {
                    // no useful data to rebroadcast
                    nng_msg_free(receivedMessage.message);
                    receivedMessage.message = nullptr;
                }

                const auto curForeignAck = (crossChainMessage.has_ack_count())
                                               ? std::optional<uint64_t>(crossChainMessage.ack_count().value())
                                               : std::nullopt;
                const auto senderStake = configuration.kOtherNetworkStakes.at(senderId);
                if (curForeignAck.has_value())
                {
                    quorumAck->updateNodeAck(senderId, senderStake, *curForeignAck);
                }
                while (not viewQueue.try_enqueue(
                           util::JankAckView{.senderId = (uint8_t)senderId,
                                             .ackOffset = (uint32_t)(curForeignAck.value_or(0ULL - 1ULL) + 2),
                                             .view = std::move(*crossChainMessage.mutable_ack_set())}) &&
                       not is_test_over())
                    ;
                // crossChainMessage.clear_ack_count();
                // crossChainMessage.clear_ack_set();
                // const auto protoSize = crossChainMessage.ByteSizeLong();
                // crossChainMessage.SerializeToArray(messageData, protoSize);
                // const auto sizeShrink = messageSize - protoSize;
                // nng_msg_chop(message, sizeShrink);
            }
        }

        if (receivedMessage.message)
        {
            bool success = pipeline->rebroadcastToOwnRsm(receivedMessage.message);
            if (success)
            {
                receivedMessage.message = nullptr;
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
            }

            for (const auto &messageData : crossChainMessage.data())
            {
                if (not util::isMessageDataValid(messageData))
                {
                    continue;
                }
                acknowledgment->addToAckList(messageData.sequence_number());
                timedMessages += is_test_recording();
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
    addMetric("foreign_messages_received", timedMessages);
    addMetric("max_acknowledgment", acknowledgment->getAckIterator().value_or(0));
    addMetric("max_quorum_acknowledgment", quorumAck->getCurrentQuack().value_or(0));
}
