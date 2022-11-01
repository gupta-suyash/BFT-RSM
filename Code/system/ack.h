#ifndef _ACKNOW_
#define _ACKNOW_

#include <list>
#include <mutex>
#include <unordered_map>

#include "global.h"

class Acknowledgment
{
  private:
    uint64_t ackValue;
    std::mutex ack_mutex;
    std::list<uint64_t> msg_recv_;

  public:
    void Init();
    void AddToAckList(uint64_t mid);
    uint64_t GetAckIterator();

    void TestFunc();
};

class QuorumAcknowledgment
{
    uint64_t quack_value_;
    std::unordered_map<uint64_t, uint16_t> quack_recv_;

  public:
    void Init();
    void QuackCheck(uint64_t min, uint64_t max);
    void AddToQuackMap(uint64_t mid);
    uint64_t GetQuackIterator();

    void TestFunc();
};

#endif
