#include <atomic>
#include <thread>
#include <chrono>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <spdlog/spdlog.h>
#include "scrooge_message.pb.h"
#include "readerwritercircularbuffer.h"
#include "readerwriterqueue.h"

const uint64_t min_size_power = 7;
const uint64_t duration = 10;

uint64_t test_integer(uint64_t max_power);
uint64_t test_stack_protobuf_queue_rate(uint64_t max_power, uint64_t msg_sz);
uint64_t test_heap_protobuf_queue_rate(uint64_t max_power, uint64_t msg_sz);
uint64_t test_protobuf_creation_stack(uint64_t string_sz);
uint64_t test_protobuf_creation_heap(uint64_t string_sz);