#pragma once

#include "data_comm.h"
#include "global.h"
#include <iostream>
#include <mutex>
#include <thread>
#include <chrono>
#include <tuple>
#include <deque>

class PipeQueue
{
  private:
    std::mutex msg_q_mutex;
    std::chrono::duration<std::chrono::milliseconds> duration;
    std::queue<scrooge::CrossChainMessage> msg_queue_;
    std::mutex store_q_mutex;
    std::deque<std::tuple<scrooge::CrossChainMessage, std::chrono::time_point<std::chrono::steady_clock>>> store_deque_;


  public:
	PipeQueue(std::chrono::duration<std::chrono::milliseconds>);
    void Enqueue(scrooge::CrossChainMessage msg);
    scrooge::CrossChainMessage Dequeue();
    scrooge::CrossChainMessage EnqueueStore();
    void DequeueStore(scrooge::CrossChainMessage msg);
    void UpdateStore();

    // Msg_Queue Testing functions.
    void CallE();
    void CallD();
    void CallThreads();
};
