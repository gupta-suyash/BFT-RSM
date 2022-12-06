#pragma once

#include "global.h"
#include "pipe_queue.h"
#include <thread>

class IOThreads
{
  public:
    uint16_t thd_id_; // Thread id.
    thread thd_;      // Thread.
    virtual void Init(uint16_t thd_id) = 0;
    virtual void Run() = 0;
    virtual uint16_t GetThreadId() = 0;
};

// Threads that send or receive messages.
class SendThread : public IOThreads
{
    uint16_t last_sent_; // Id of node from other RSM.
    uint64_t num_packets = 300;
  public:
    // uint16_t thd_id_; // Thread id.
    // thread thd_;	// Thread.
    void Init(uint16_t thd_id);
    void Run();
    uint16_t GetThreadId();

    uint16_t GetLastSent();
    void SetLastSent(uint16_t id);

    void TestAddBlockToInQueue(const uint64_t bid);
};

class RecvThread : public IOThreads
{
  public:
    // uint16_t thd_id_; // Thread id.
    // thread thd_;	// Thread.
    void Init(uint16_t thd_id);
    void Run();
    uint16_t GetThreadId();
};
