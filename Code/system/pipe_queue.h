#pragma once

#include "data_comm.h"
#include "global.h"
#include <iostream>
#include <mutex>
#include <thread>
#include <chrono>
#include <tuple>

class PipeQueue
{
  private:
    std::mutex msg_q_mutex;
    std::chrono::duration<double> duration;
    std::queue<scrooge::CrossChainMessage> msg_queue_;
    std::mutex store_q_mutex;
    std::queue<std::tuple<scrooge::CrossChainMessage, std::chrono::time_point<std::chrono::steady_clock>>> store_queue_;


  public:
    void Enqueue(scrooge::CrossChainMessage msg);
    scrooge::CrossChainMessage Dequeue();
    scrooge::CrossChainMessage EnqueueStore();
    scrooge::CrossChainMessage DequeueStore(scrooge::CrossChainMessage msg);
    int CheckTime(uint64_t sequence_id);
    scrooge::CrossChainMessage UpdateStore(scrooge::CrossChainMessage msg, uint64_t sequence_id);

    // Msg_Queue Testing functions.
    void CallE();
    void CallD();
    void CallThreads();
};
