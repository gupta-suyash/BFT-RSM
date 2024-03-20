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

boost::circular_buffer<scrooge::MessageResendData> resendDatas(1 << 16);
boost::circular_buffer<acknowledgment_tracker::ResendData> requestedResends(1 << 16);

auto lastSendTime = std::chrono::steady_clock::now();
uint64_t numMsgsSentWithLastAck{};
std::optional<uint64_t> lastSentAck{};
uint64_t lastQuack = 0;
constexpr uint64_t kAckWindowSize = 1000;
constexpr uint64_t kQAckWindowSize = 1000 * 1000000;
// Optimal window size for non-stake: 12*16 and for stake: 12*8
constexpr auto kMaxMessageDelay = 1us;
constexpr auto kNoopDelay = 800us;
uint64_t noop_ack = 0;
uint64_t numResendChecks{}, numActiveResends{}, numResendsOverQuack{}, numMessagesSent{}, numResendsTooHigh{},
    numResendsTooLow{}, searchDistance{}, searchChecks{};
uint64_t numQuackWindowFails{}, numAckWindowFails{}, numSendChecks{}, numIOTimeoutHits{}, numNoopTimeoutHits{},
    numTimeoutExclusiveHits{};
bool isResendDataUpdated{};
uint64_t maxResendRequest{};
uint64_t numMessagesResent{};
static uint64_t counter = 0;

uint64_t testtrueMod(int64_t value, int64_t modulus)
{
    const auto remainder = (value % modulus);
    return (remainder < 0) ? remainder + modulus : remainder;
}

inline uint64_t teststakeToNode(uint64_t stakeIndex, const std::vector<uint64_t> &networkStakePrefixSum)
{
    for (size_t offset{1};; offset++)
    {
        if (stakeIndex < networkStakePrefixSum[offset])
        {
            return offset - 1;
        }
    }
}

inline std::optional<uint64_t> MessageScheduler::getResendNumber(uint64_t sequenceNumber) const
{
    const auto roundOffset = sequenceNumber / kOwnApportionedStake;
    const auto originalApportionedSendNode =
        teststakeToNode(sequenceNumber % kOwnApportionedStake, kOwnRsmApportionedStakePrefixSum);
    const auto originalApportionedRecvNode =
        teststakeToNode((sequenceNumber + roundOffset) % kOtherApportionedStake, kOtherRsmApportionedStakePrefixSum);

    std::optional<std::optional<uint64_t>> *const lookupEntry =
        mResendNumberLookup[originalApportionedSendNode].data() + originalApportionedRecvNode;

    if (lookupEntry->has_value())
    {
        return lookupEntry->value();
    }

    const auto originalSender = kOwnRsmApportionedStakePrefixSum.at(originalApportionedSendNode);
    const auto originalReceiver = kOtherRsmApportionedStakePrefixSum.at(originalApportionedRecvNode);
    const auto ownNodeFirstStake = kOwnRsmStakePrefixSum.at(kOwnNodeId);
    const auto ownNodeLastStake = kOwnRsmStakePrefixSum.at(kOwnNodeId + 1) - 1;
    const auto isNodeFirstSender = ownNodeFirstStake <= originalSender && originalSender <= ownNodeLastStake;
    const auto ownNodeFirstSentStake = (isNodeFirstSender) ? originalSender : ownNodeFirstStake;
    const auto previousStakeSent = testtrueMod((int64_t)ownNodeFirstSentStake - (int64_t)originalSender, kStakePerRsm);

    const auto isOwnNodeNotASender = previousStakeSent >= kMinStakeToSend;
    if (isOwnNodeNotASender)
    {
        *lookupEntry = std::optional<uint64_t>{};
        return std::nullopt;
    }
    const auto ownNodeFirstReceiver = (originalReceiver + previousStakeSent) % kStakePerRsm;

    const auto originalSenderId = teststakeToNode(originalSender, kOwnRsmStakePrefixSum);
    const auto originalReceiverId = teststakeToNode(originalReceiver, kOtherRsmStakePrefixSum);
    const auto ownFirstSenderId = kOwnNodeId;
    const auto ownFirstReceiverId = teststakeToNode(ownNodeFirstReceiver, kOtherRsmStakePrefixSum);

    const auto priorSenders = testtrueMod((int64_t)ownFirstSenderId - (int64_t)originalSenderId, kOwnNetworkSize);
    const auto priorReceivers =
        testtrueMod((int64_t)ownFirstReceiverId - (int64_t)originalReceiverId, kOtherNetworkSize);

    *lookupEntry = std::max(priorSenders, priorReceivers);
    return lookupEntry->value();
}

inline message_scheduler::CompactDestinationList MessageScheduler::getMessageDestinations(uint64_t sequenceNumber) const
{
    const auto roundOffset = sequenceNumber / kOwnApportionedStake;
    const auto originalApportionedSendNode =
        teststakeToNode(sequenceNumber % kOwnApportionedStake, kOwnRsmApportionedStakePrefixSum);
    const auto originalApportionedRecvNode =
        teststakeToNode((sequenceNumber + roundOffset) % kOtherApportionedStake, kOtherRsmApportionedStakePrefixSum);

    std::optional<message_scheduler::CompactDestinationList> *const lookupEntry =
        mResendDestinationLookup[originalApportionedSendNode].data() + originalApportionedRecvNode;

    if (lookupEntry->has_value())
    {
        return lookupEntry->value();
    }

    // Algorithm : do all send/recv math with stake, then call teststakeToNode
    const auto originalSender = kOwnRsmApportionedStakePrefixSum.at(originalApportionedSendNode);
    const auto originalReceiver = kOtherRsmApportionedStakePrefixSum.at(originalApportionedRecvNode);
    const auto finalSender = (sequenceNumber + kMinStakeToSend - 1) % kStakePerRsm;
    const auto ownNodeFirstStake = kOwnRsmStakePrefixSum.at(kOwnNodeId);
    const auto ownNodeLastStake = kOwnRsmStakePrefixSum.at(kOwnNodeId + 1) - 1;
    const auto isNodeFirstSender = ownNodeFirstStake <= originalSender && originalSender <= ownNodeLastStake;
    const auto ownNodeFirstSentStake = (isNodeFirstSender) ? originalSender : ownNodeFirstStake;
    const auto previousStakeSent = testtrueMod((int64_t)ownNodeFirstSentStake - (int64_t)originalSender, kStakePerRsm);

    const auto isOwnNodeNotASender = previousStakeSent >= kMinStakeToSend;
    if (isOwnNodeNotASender)
    {
        *lookupEntry = message_scheduler::CompactDestinationList{};
        return message_scheduler::CompactDestinationList{};
    }

    const auto isNodeCutoff = (ownNodeFirstSentStake <= finalSender && finalSender < ownNodeLastStake);
    const auto ownNodeFinalSentStake = (isNodeCutoff) ? finalSender : ownNodeLastStake;
    const auto stakeSentByOwnNode = ownNodeFinalSentStake - ownNodeFirstSentStake + 1;
    message_scheduler::CompactDestinationList destinations{};

    int64_t stakeLeftToSend = stakeSentByOwnNode;
    auto curReceiverStake = (originalReceiver + previousStakeSent) % kStakePerRsm;
    auto curReceiverId = teststakeToNode(curReceiverStake, kOtherRsmStakePrefixSum);
    while (stakeLeftToSend > 0)
    {
        // using uint16_t is a petty optimization and can be removed anytime :whistling:
        destinations.push_back((uint16_t)curReceiverId);

        const auto stakeSentToCurReceiver = kOtherRsmStakePrefixSum.at(curReceiverId + 1) - curReceiverStake;

        stakeLeftToSend -= stakeSentToCurReceiver;
        curReceiverStake = (curReceiverStake + stakeSentToCurReceiver) % kStakePerRsm;
        curReceiverId = (curReceiverId + 1 == kOtherNetworkSize) ? 0 : curReceiverId + 1;
    }
    *lookupEntry = destinations;
    return destinations;
}

template <bool kIsUsingFile>
bool handleNewMessage(std::chrono::steady_clock::time_point curTime, const MessageScheduler &messageScheduler,
                      std::optional<uint64_t> curQuack, Pipeline *const pipeline,
                      const Acknowledgment *const acknowledgment, scrooge::CrossChainMessageData &&newMessageData)
{
    const auto sequenceNumber = newMessageData.sequence_number();

    uint64_t checkVal = curQuack.value_or(0);
    lastQuack = checkVal;

    const auto resendNumber = messageScheduler.getResendNumber(sequenceNumber);
    const auto isMessageNeverSent = not resendNumber.has_value();
    if (isMessageNeverSent)
    {
        //SPDLOG_CRITICAL("MESSAGE {} NEVER SENT", sequenceNumber);
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
                  //  SPDLOG_CRITICAL("POSSIBLY LATE Sending to OTHER RSM: RECV NODE {} SN {} ACK {}", receiverNode, sequenceNumber, acknowledgment->getAckIterator().value_or(0));
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
                //SPDLOG_CRITICAL("FIRST TIME Sending to OTHER RSM: RECV NODE {} SN {} ACK {}", receiverNode, sequenceNumber, acknowledgment->getAckIterator().value_or(0));
            }
            if (isMessageSent)
            {
                lastSendTime = curTime;
                numMsgsSentWithLastAck++;
            }
        }
    } else {
        SPDLOG_CRITICAL("NOT FIRST SEND: {} isSentLater {}", resendNumber.value_or(0), isPossiblySentLater);
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
    //SPDLOG_CRITICAL("HANDLING: MESSAGE #{} with: SEQNO {} CURQUACK {}", counter, sequenceNumber, lastQuack);
    counter += 1;
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

    acknowledgment_tracker::ResendData activeResend;
    const auto minimumResendSequenceNumber =
        (resendDatas.size()) ? resendDatas.front().sequenceNumber : curQuack.value_or(-1ULL) + 1;
    while (not requestedResends.full() && resendDataQueue->try_dequeue(activeResend))
    {
        if (activeResend.sequenceNumber >= minimumResendSequenceNumber)
        {
            requestedResends.push_back(activeResend);
            // SPDLOG_CRITICAL("ADDING A REQUESTED RESEND S={} Quack={}", activeResend.sequenceNumber,
            // curQuack.value_or(0));
            isResendDataUpdated = true;
            maxResendRequest = std::max<uint64_t>(maxResendRequest, activeResend.sequenceNumber);
        }
    }

    while (resendDatas.size() && (resendDatas.front().sequenceNumber <= curQuack ||
                                  resendDatas.front().messageData.message_content().empty()))
    {
        // SPDLOG_CRITICAL("Removing resend data with s# {} Quack={}", resendDatas.front().sequenceNumber,
        // curQuack.value_or(0));
        resendDatas.pop_front();
    }

    while (requestedResends.size() && (not requestedResends.front().isActive ||
                                       requestedResends.front().sequenceNumber < minimumResendSequenceNumber))
    {
        // SPDLOG_CRITICAL("REMOVING A REQUESTED RESEND S{}, Quack={} minS#={}",
        // requestedResends.front().sequenceNumber, curQuack.value_or(0), minimumResendSequenceNumber);
        requestedResends.pop_front();
    }
}

template <bool kIsUsingFile>
void handleResends(std::chrono::steady_clock::time_point curTime, Pipeline *const pipeline,
                   const Acknowledgment *const acknowledgment)
{

    auto pseudoBegin = std::begin(resendDatas);
    uint64_t prevActiveResendSequenceNum = 0;

    std::bitset<32> destinationsToFlush{};
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
            // couldn't find anything
            continue;
        }

        const bool noResendDataFound = curResendDataIt->sequenceNumber > requestedResend.sequenceNumber;
        if (noResendDataFound)
        {
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
                bool isFlushed{};
                if constexpr (kIsUsingFile)
                {
                    pipeline->SendFileToOtherRsm(destination, std::move(messageDataCopy), acknowledgment, curTime);
                }
                else
                {
                    pipeline->SendToOtherRsm(destination, std::move(messageDataCopy), acknowledgment, curTime);
                }
                destinationsToFlush[destination] = not isFlushed;
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
                destinationsToFlush[destination] = not isFlushed;
                requestedResend.isActive = false;
                assert(messageData.message_content().empty());
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
            const bool shouldContinue = handleNewMessage<kIsUsingFile>(
                curTime, messageScheduler, curQuack, pipeline.get(), acknowledgment.get(), std::move(newMessageData));
            if (shouldContinue)
            {
                continue;
            }
        }
        else if (isAckFresh && isNoopTimeoutHit) // Always send no-ops, maybe not enough messages to flush buffers?
        {
            static uint64_t receiver = 0;

            if constexpr (kIsUsingFile)
            {
                pipeline->forceSendFileToOtherRsm(receiver % kOtherNetworkSize, acknowledgment.get(), curTime);
            }
            else
            {
                //SPDLOG_CRITICAL("Are we force sending to other rsm?");
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

        handleResends<kIsUsingFile>(curTime, pipeline.get(), acknowledgment.get());

        uint64_t numDeletes = 0;
        for (; numDeletes < resendDatas.size(); numDeletes++)
        {
            const auto &curResend = resendDatas[numDeletes];
            if (curResend.numDestinationsSent != curResend.destinations.size())
            {
                break;
            }
        }
        if (numDeletes)
            resendDatas.erase_begin(numDeletes);
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

    constexpr auto kNumAckTrackers = kListSize * 3;
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

        // if (is_test_over())
        // {
        //     return;
        // } // commenting this out is a bug but doesn't really matter since test is over

        const auto curForeignAck = util::getAckIterator(curView);

        const auto senderStake = configuration.kOtherNetworkStakes.at(curView.senderId);
        if (curForeignAck.has_value())
        {
            quorumAck->updateNodeAck(curView.senderId, senderStake, *curForeignAck);
        }

        const auto currentQuack = quorumAck->getCurrentQuack();
        updateAckTrackers(currentQuack, senderStake, curView, ackTrackers, resendDataQueue, messageScheduler);
    }
    SPDLOG_CRITICAL("LAME THREAD ENDING");
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
                        SPDLOG_CRITICAL("Message Data is invalid!");
                        continue;
                    }
                    acknowledgment->addToAckList(messageData.sequence_number());
                    //SPDLOG_CRITICAL("SEQUENCE NUMBER ADDED TO ACK LIST 790: {} SenderId {}", messageData.sequence_number(), senderId);
                    timedMessages += is_test_recording();
                }

                const auto curForeignAck = (crossChainMessage.has_ack_count())
                                               ? std::optional<uint64_t>(crossChainMessage.ack_count().value())
                                               : std::nullopt;

                while (not viewQueue.try_enqueue(
                           util::JankAckView{.senderId = (uint8_t)senderId,
                                             .ackOffset = (uint32_t)(curForeignAck.value_or(0ULL - 1ULL) + 2),
                                             .view = std::move(*crossChainMessage.mutable_ack_set())}) &&
                       not is_test_over())
                    ;
            }
        }

        if (receivedMessage.message)
        {
            bool success = pipeline->rebroadcastToOwnRsm(receivedMessage.message);
            if (success)
            {
                //SPDLOG_CRITICAL("SENT MESSAGE WITH SEQUENCE ID {} TO OWN RSM", receivedMessage.message);
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
                    SPDLOG_CRITICAL("Message is invalid!");
                    continue;
                }
                acknowledgment->addToAckList(messageData.sequence_number());
                //SPDLOG_CRITICAL("SEQUENCE NUMBER ADDED TO ACK LIST 840: {}", messageData.sequence_number()); 
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
