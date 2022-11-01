#include "ack.h"

#include <limits>

/* Appends max integer to the list and ackValue is set to this max integer.
 *
 */
void Acknowledgment::Init()
{
    // The first entry to the list is not a msg identifier (max int).
    msg_recv_.push_back(std::numeric_limits<uint64_t>::max());
    ackValue = std::numeric_limits<uint64_t>::max();
}

/* Adds an element to the lsit msg_recv_.
 * We also do a sliding window style operation where consecutive acks are deleted.
 *
 * @param mid is the value to be added
 */
void Acknowledgment::AddToAckList(uint64_t mid)
{
    auto it = msg_recv_.begin();

    // If the first element is max value, remove it and add mid.
    if (*it == std::numeric_limits<uint64_t>::max())
    {
        msg_recv_.push_back(mid);
        it = msg_recv_.erase(it);
    }
    else
    {
        // Flag to check if mid is added at the end of the list.
        bool last_flag = true;
        for (; it != msg_recv_.end(); ++it)
        {
            // cout << "Compare Value: " << *it << endl;
            if (*it > mid)
            {
                // Insert before the element larger than mid.
                msg_recv_.emplace(it, mid);
                last_flag = false;
                break;
            }
        }

        if (last_flag)
        {
            // Appending to the list.
            msg_recv_.push_back(mid);
        }
    }

    // Need to lock accesses to ackValue as it used by multiple threads
    ack_mutex.lock();

    // cout << "AckValue: " << ackValue << " :: mid: " << mid << endl;
    if (mid == 1)
    {
        // If mid = 1, set the ackValue to 1.
        ackValue = 1;
    }

    // Enter if ackValue is not std::numeric_limits<uint64_t>::max(); possible if mid=1, not inserted yet.
    if (ackValue != std::numeric_limits<uint64_t>::max())
    {
        // Flag to determine if we need to delete old acknowledgments.
        bool del_flag = false;
        it = msg_recv_.begin();
        it++;
        for (; it != msg_recv_.end(); it++)
        {
            if (*it != ackValue + 1)
            {
                break;
            }
            ackValue++;
            del_flag = true;
        }

        if (del_flag)
        {
            // Delete from beginning to the element less than ackValue.
            --it;
            msg_recv_.erase(msg_recv_.begin(), it);
        }
    }

    // Unlocking the mutex.
    ack_mutex.unlock();
}

/* Get the value of variable ackValue; needs to be locked as multi-threaded access.
 *
 * @return ackValue
 */
uint64_t Acknowledgment::GetAckIterator()
{
    uint64_t aval;

    // Protecting accesses to the mutex.
    ack_mutex.lock();
    aval = ackValue;
    ack_mutex.unlock();

    return aval;
}

/* Appends max integer to the map and quack_value is set to this max integer.
 *
 */
void QuorumAcknowledgment::Init()
{
    // The first entry to the list is not a msg identifier (max int).
    quack_recv_.emplace(std::numeric_limits<uint64_t>::max(), 0);
    quack_value_ = std::numeric_limits<uint64_t>::max();
}

/* Adds an element to the map quack_recv_.
 * We also do a sliding window style operation where consecutive quacks are deleted.
 *
 * @param aid is the value to be added
 */
void QuorumAcknowledgment::AddToQuackMap(uint64_t aid)
{
    cout << "QV: " << quack_value_ << endl;

    if (quack_value_ == std::numeric_limits<uint64_t>::max())
    {
        quack_recv_[std::numeric_limits<uint64_t>::max()] = quack_recv_[std::numeric_limits<uint64_t>::max()] + 1;

        // if(aid == 0) {
        //	auto search = quack_recv_.find(0);
        //	if(search != quack_recv_.end()) {
        //		search->second++;
        //	}
        //}

        QuackCheck(0, aid);

        // Removing the std::numeric_limits<uint64_t>::max(), the default entry.
        if (quack_value_ != std::numeric_limits<uint64_t>::max())
        {
            quack_recv_.erase(std::numeric_limits<uint64_t>::max());
        }
    }
    else
    {
        QuackCheck(quack_value_, aid);
    }
}

/* This is a helper function that helps to add an entry (if does not exist)
 * for each received ack in the range min to max. If an entry already exists,
 * increment the counter. Post increment, we check if for any aid the
 * quack_value_ has reached f+1. If so, we update the quack_value, and
 * delete any lower entries.
 *
 * @param min is the minimum value to start.
 * @param max is the last value for check.
 *
 */
void QuorumAcknowledgment::QuackCheck(uint64_t min, uint64_t max)
{
    bool quack_update = false;
    for (uint64_t i = min; i <= max; i++)
    {
        auto search = quack_recv_.find(i);
        if (search != quack_recv_.end())
        {
            search->second = search->second + 1;
            cout << "Found: " << search->first << " :: " << search->second << endl;
        }
        else
        {
            quack_recv_.emplace(i, 1);
        }
    }

    for (uint64_t i = min; i <= max; i++)
    {
        // When f+1 quacks hve been received.
        if (quack_recv_[i] == get_max_nodes_fail() + 1)
        {
            // Updating the quack_value.
            quack_value_ = i;
            quack_update = true;
            cout << "New Quack: " << quack_value_ << endl;
        }
    }

    if (quack_update)
    {
        for (uint64_t i = min; i < quack_value_; i++)
        {
            // Erasing lower value entries based on the key.
            cout << "Erasing: " << i << endl;
            quack_recv_.erase(i);
        }
    }
}

/* Get the value of variable quackValue.
 *
 * @return quackValue
 */
uint64_t QuorumAcknowledgment::GetQuackIterator()
{
    return quack_value_;
}

/* This function is just to test the functionality in a single-threaded execution.
 *
 */
void QuorumAcknowledgment::TestFunc()
{
    Init();

    /*
     * TEST1
     *
    AddToQuackMap(0);
    cout << "Updated QuackValue: " << quack_value_ << endl;

    AddToQuackMap(4);
    cout << "Updated QuackValue: " << quack_value_ << endl;

    AddToQuackMap(3);
    cout << "Updated QuackValue: " << quack_value_ << endl;

    AddToQuackMap(7);
    cout << "Updated QuackValue: " << quack_value_ << endl;

    AddToQuackMap(7);
    cout << "Updated QuackValue: " << quack_value_ << endl;
    */

    AddToQuackMap(7);
    cout << "Updated QuackValue: " << quack_value_ << endl;

    AddToQuackMap(4);
    cout << "Updated QuackValue: " << quack_value_ << endl;
}

/* This function is just to test the functionality in a single-threaded execution.
 *
 */
void Acknowledgment::TestFunc()
{
    /*
     * Test 1
     *
    AddToAckList(0);
    cout << "AckValue is: " << GetAckIterator() << " :: First element: " << *(msg_recv_.begin()) << " :: Size: " <<
    msg_recv_.size() << endl;

    AddToAckList(1);
    cout << "AckValue is: " << GetAckIterator() << " :: First element: " << *(msg_recv_.begin()) << " :: Size: " <<
    msg_recv_.size() << endl;

    AddToAckList(10);
    cout << "AckValue is: " << GetAckIterator() << " :: First element: " << *(msg_recv_.begin()) << " :: Size: " <<
    msg_recv_.size() << endl;


    AddToAckList(11);
    cout << "AckValue is: " << GetAckIterator() << " :: First element: " << *(msg_recv_.begin()) << " :: Size: " <<
    msg_recv_.size() << endl;

    AddToAckList(4);
    cout << "AckValue is: " << GetAckIterator() << " :: First element: " << *(msg_recv_.begin()) << " :: Size: " <<
    msg_recv_.size() << endl;

    AddToAckList(3);
    cout << "AckValue is: " << GetAckIterator() << " :: First element: " << *(msg_recv_.begin()) << " :: Size: " <<
    msg_recv_.size() << endl;

    AddToAckList(2);
    cout << "AckValue is: " << GetAckIterator() << " :: First element: " << *(msg_recv_.begin()) << " :: Size: " <<
    msg_recv_.size() << endl;
    */

    /*
     * Test 2
     *
    AddToAckList(0);
    cout << "AckValue is: " << GetAckIterator() << " :: First element: " << *(msg_recv_.begin()) << " :: Size: " <<
    msg_recv_.size() << endl;

    AddToAckList(10);
    cout << "AckValue is: " << GetAckIterator() << " :: First element: " << *(msg_recv_.begin()) << " :: Size: " <<
    msg_recv_.size() << endl;

    AddToAckList(11);
    cout << "AckValue is: " << GetAckIterator() << " :: First element: " << *(msg_recv_.begin()) << " :: Size: " <<
    msg_recv_.size() << endl;

    AddToAckList(1);
    cout << "AckValue is: " << GetAckIterator() << " :: First element: " << *(msg_recv_.begin()) << " :: Size: " <<
    msg_recv_.size() << endl;
    */

    /*
     * Test 3
     */
    AddToAckList(0);
    cout << "AckValue is: " << GetAckIterator() << " :: First element: " << *(msg_recv_.begin())
         << " :: Size: " << msg_recv_.size() << endl;

    AddToAckList(2);
    cout << "AckValue is: " << GetAckIterator() << " :: First element: " << *(msg_recv_.begin())
         << " :: Size: " << msg_recv_.size() << endl;

    AddToAckList(3);
    cout << "AckValue is: " << GetAckIterator() << " :: First element: " << *(msg_recv_.begin())
         << " :: Size: " << msg_recv_.size() << endl;

    AddToAckList(1);
    cout << "AckValue is: " << GetAckIterator() << " :: First element: " << *(msg_recv_.begin())
         << " :: Size: " << msg_recv_.size() << endl;
}
