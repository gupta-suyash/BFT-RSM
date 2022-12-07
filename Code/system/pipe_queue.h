#pragma once

#include "data_comm.h"
#include "global.h"
#include <chrono>
#include <deque>
#include <iostream>
#include <mutex>
#include <thread>
#include <tuple>
#include <vector>

class PipeQueue
{
  private:
    std::mutex msg_q_mutex;
    std::chrono::duration<double> duration;
    std::queue<scrooge::CrossChainMessage> msg_queue_;
    std::mutex store_q_mutex;
    std::deque<std::tuple<scrooge::CrossChainMessage, std::chrono::time_point<std::chrono::steady_clock>>> store_deque_;

  public:
    PipeQueue(std::chrono::duration<double> wait_time);
    void Enqueue(scrooge::CrossChainMessage msg);
    scrooge::CrossChainMessage Dequeue();
    scrooge::CrossChainMessage EnqueueStore();
    void DequeueStore(scrooge::CrossChainMessage msg);
    std::vector<scrooge::CrossChainMessage> UpdateStore();

    // Msg_Queue Testing functions.
    void CallE();
    void CallD();
    void CallThreads();
};
