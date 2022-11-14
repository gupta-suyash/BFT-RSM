#pragma once

#include "data_comm.h"
#include "global.h"
#include <iostream>
#include <mutex>
#include <thread>

class PipeQueue
{
  private:
    std::mutex msg_q_mutex;
    std::queue<scrooge::CrossChainMessage> msg_queue_;
    std::mutex store_q_mutex;
    std::queue<scrooge::CrossChainMessage> store_queue_;

  public:
    void Enqueue(scrooge::CrossChainMessage msg);
    scrooge::CrossChainMessage Dequeue();
    scrooge::CrossChainMessage EnqueueStore();

    // Msg_Queue Testing functions.
    void CallE();
    void CallD();
    void CallThreads();
};
