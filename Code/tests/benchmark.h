#include <atomic>
#include <thread>
#include <chrono>
#include <stdlib.h>
#include <google/protobuf/arena.h>
#include "../system/readerwritercircularbuffer.h"
#include "../system/readerwriterqueue.h"

uint64_t test_integer(uint64_t max_power);
uint64_t test_protobuf_queue_rate(uint64_t max_power);
uint64_t test_protobuf_ptrs_queue_rate(uint64_t max_power);
uint64_t test_protobuf_creation_stack(uint64_t string_sz);
uint64_t test_protobuf_creation_heap(uint64_t string_sz);