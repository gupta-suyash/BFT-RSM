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
    std::queue<std::tuple<scrooge::CrossChainMessage, static std::chrono::time_point>> store_queue_;


  public:
    void Enqueue(scrooge::CrossChainMessage msg);
    scrooge::CrossChainMessage Dequeue();
    scrooge::CrossChainMessage EnqueueStore();
	scrooge::CrossChainMessage DequeueStore();
	int CheckTime();
	scrooge::CrossChainMessage UpdateStore();

    // Msg_Queue Testing functions.
    void CallE();
    void CallD();
    void CallThreads();
};
