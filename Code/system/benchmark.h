// Copyright 2015 Google Inc. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Support for registering benchmarks for functions.

/* Example usage:
// Define a function that executes the code to be measured a
// specified number of times:
static void BM_StringCreation(benchmark::State& state) {
  for (auto _ : state)
    std::string empty_string;
}
// Register the function as a benchmark
BENCHMARK(BM_StringCreation);
// Define another benchmark
static void BM_StringCopy(benchmark::State& state) {
  std::string x = "hello";
  for (auto _ : state)
    std::string copy(x);
}
BENCHMARK(BM_StringCopy);
// Augment the main() program to invoke benchmarks if specified
// via the --benchmark_filter command line flag.  E.g.,
//       my_unittest --benchmark_filter=all
//       my_unittest --benchmark_filter=BM_StringCreation
//       my_unittest --benchmark_filter=String
//       my_unittest --benchmark_filter='Copy|Creation'
int main(int argc, char** argv) {
  benchmark::Initialize(&argc, argv);
  benchmark::RunSpecifiedBenchmarks();
  benchmark::Shutdown();
  return 0;
}
// Sometimes a family of microbenchmarks can be implemented with
// just one routine that takes an extra argument to specify which
// one of the family of benchmarks to run.  For example, the following
// code defines a family of microbenchmarks for measuring the speed
// of memcpy() calls of different lengths:
static void BM_memcpy(benchmark::State& state) {
  char* src = new char[state.range(0)]; char* dst = new char[state.range(0)];
  memset(src, 'x', state.range(0));
  for (auto _ : state)
    memcpy(dst, src, state.range(0));
  state.SetBytesProcessed(state.iterations() * state.range(0));
  delete[] src; delete[] dst;
}
BENCHMARK(BM_memcpy)->Arg(8)->Arg(64)->Arg(512)->Arg(1<<10)->Arg(8<<10);
// The preceding code is quite repetitive, and can be replaced with the
// following short-hand.  The following invocation will pick a few
// appropriate arguments in the specified range and will generate a
// microbenchmark for each such argument.
BENCHMARK(BM_memcpy)->Range(8, 8<<10);
// You might have a microbenchmark that depends on two inputs.  For
// example, the following code defines a family of microbenchmarks for
// measuring the speed of set insertion.
static void BM_SetInsert(benchmark::State& state) {
  set<int> data;
  for (auto _ : state) {
    state.PauseTiming();
    data = ConstructRandomSet(state.range(0));
    state.ResumeTiming();
    for (int j = 0; j < state.range(1); ++j)
      data.insert(RandomNumber());
  }
}
BENCHMARK(BM_SetInsert)
   ->Args({1<<10, 128})
   ->Args({2<<10, 128})
   ->Args({4<<10, 128})
   ->Args({8<<10, 128})
   ->Args({1<<10, 512})
   ->Args({2<<10, 512})
   ->Args({4<<10, 512})
   ->Args({8<<10, 512});
// The preceding code is quite repetitive, and can be replaced with
// the following short-hand.  The following macro will pick a few
// appropriate arguments in the product of the two specified ranges
// and will generate a microbenchmark for each such pair.
BENCHMARK(BM_SetInsert)->Ranges({{1<<10, 8<<10}, {128, 512}});
// For more complex patterns of inputs, passing a custom function
// to Apply allows programmatic specification of an
// arbitrary set of arguments to run the microbenchmark on.
// The following example enumerates a dense range on
// one parameter, and a sparse range on the second.
static void CustomArguments(benchmark::internal::Benchmark* b) {
  for (int i = 0; i <= 10; ++i)
    for (int j = 32; j <= 1024*1024; j *= 8)
      b->Args({i, j});
}
BENCHMARK(BM_SetInsert)->Apply(CustomArguments);
// Templated microbenchmarks work the same way:
// Produce then consume 'size' messages 'iters' times
// Measures throughput in the absence of multiprogramming.
template <class Q> int BM_Sequential(benchmark::State& state) {
  Q q;
  typename Q::value_type v;
  for (auto _ : state) {
    for (int i = state.range(0); i--; )
      q.push(v);
    for (int e = state.range(0); e--; )
      q.Wait(&v);
  }
  // actually messages, not bytes:
  state.SetBytesProcessed(state.iterations() * state.range(0));
}
BENCHMARK_TEMPLATE(BM_Sequential, WaitQueue<int>)->Range(1<<0, 1<<10);
Use `Benchmark::MinTime(double t)` to set the minimum time used to run the
benchmark. This option overrides the `benchmark_min_time` flag.
void BM_test(benchmark::State& state) {
 ... body ...
}
BENCHMARK(BM_test)->MinTime(2.0); // Run for at least 2 seconds.
In a multithreaded test, it is guaranteed that none of the threads will start
until all have reached the loop start, and all will have finished before any
thread exits the loop body. As such, any global setup or teardown you want to
do can be wrapped in a check against the thread index:
static void BM_MultiThreaded(benchmark::State& state) {
  if (state.thread_index() == 0) {
    // Setup code here.
  }
  for (auto _ : state) {
    // Run the test as normal.
  }
  if (state.thread_index() == 0) {
    // Teardown code here.
  }
}
BENCHMARK(BM_MultiThreaded)->Threads(4);
If a benchmark runs a few milliseconds it may be hard to visually compare the
measured times, since the output data is given in nanoseconds per default. In
order to manually set the time unit, you can specify it manually:
BENCHMARK(BM_test)->Unit(benchmark::kMillisecond);
*/


// The _MSVC_LANG check should detect Visual Studio 2015 Update 3 and newer.
#if __cplusplus >= 201103L || (defined(_MSVC_LANG) && _MSVC_LANG >= 201103L)
#define BENCHMARK_HAS_CXX11
#endif

// This _MSC_VER check should detect VS 2017 v15.3 and newer.
#if __cplusplus >= 201703L || \
    (defined(_MSC_VER) && _MSC_VER >= 1911 && _MSVC_LANG >= 201703L)
#define BENCHMARK_HAS_CXX17
#endif

#include <stdint.h>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <iosfwd>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>


#if defined(BENCHMARK_HAS_CXX11)
#include <atomic>
#include <initializer_list>
#include <type_traits>
#include <utility>
#endif

#if defined(_MSC_VER)
#include <intrin.h>  // for _ReadWriteBarrier
#endif

#ifndef BENCHMARK_HAS_CXX11
#define BENCHMARK_DISALLOW_COPY_AND_ASSIGN(TypeName) \
  TypeName(const TypeName&);                         \
  TypeName& operator=(const TypeName&)
#else
#define BENCHMARK_DISALLOW_COPY_AND_ASSIGN(TypeName) \
  TypeName(const TypeName&) = delete;                \
  TypeName& operator=(const TypeName&) = delete
#endif

#ifdef BENCHMARK_HAS_CXX17
#define BENCHMARK_UNUSED [[maybe_unused]]
#elif defined(__GNUC__) || defined(__clang__)
#define BENCHMARK_UNUSED __attribute__((unused))
#else
#define BENCHMARK_UNUSED
#endif

// Used to annotate functions, methods and classes so they
// are not optimized by the compiler. Useful for tests
// where you expect loops to stay in place churning cycles
#if defined(__clang__)
#define BENCHMARK_DONT_OPTIMIZE __attribute__((optnone))
#elif defined(__GNUC__) || defined(__GNUG__)
#define BENCHMARK_DONT_OPTIMIZE __attribute__((optimize(0)))
#else
// MSVC & Intel do not have a no-optimize attribute, only line pragmas
#define BENCHMARK_DONT_OPTIMIZE
#endif

#if defined(__GNUC__) || defined(__clang__)
#define BENCHMARK_ALWAYS_INLINE __attribute__((always_inline))
#elif defined(_MSC_VER) && !defined(__clang__)
#define BENCHMARK_ALWAYS_INLINE __forceinline
#define __func__ __FUNCTION__
#else
#define BENCHMARK_ALWAYS_INLINE
#endif

#define BENCHMARK_INTERNAL_TOSTRING2(x) #x
#define BENCHMARK_INTERNAL_TOSTRING(x) BENCHMARK_INTERNAL_TOSTRING2(x)

// clang-format off
#if (defined(__GNUC__) && !defined(__NVCC__) && !defined(__NVCOMPILER)) || defined(__clang__)
#define BENCHMARK_BUILTIN_EXPECT(x, y) __builtin_expect(x, y)
#define BENCHMARK_DEPRECATED_MSG(msg) __attribute__((deprecated(msg)))
#define BENCHMARK_DISABLE_DEPRECATED_WARNING \
  _Pragma("GCC diagnostic push")             \
  _Pragma("GCC diagnostic ignored \"-Wdeprecated-declarations\"")
#define BENCHMARK_RESTORE_DEPRECATED_WARNING _Pragma("GCC diagnostic pop")
#elif defined(__NVCOMPILER)
#define BENCHMARK_BUILTIN_EXPECT(x, y) __builtin_expect(x, y)
#define BENCHMARK_DEPRECATED_MSG(msg) __attribute__((deprecated(msg)))
#define BENCHMARK_DISABLE_DEPRECATED_WARNING \
  _Pragma("diagnostic push") \
  _Pragma("diag_suppress deprecated_entity_with_custom_message")
#define BENCHMARK_RESTORE_DEPRECATED_WARNING _Pragma("diagnostic pop")
#else
#define BENCHMARK_BUILTIN_EXPECT(x, y) x
#define BENCHMARK_DEPRECATED_MSG(msg)
#define BENCHMARK_WARNING_MSG(msg)                           \
  __pragma(message(__FILE__ "(" BENCHMARK_INTERNAL_TOSTRING( \
      __LINE__) ") : warning note: " msg))
#define BENCHMARK_DISABLE_DEPRECATED_WARNING
#define BENCHMARK_RESTORE_DEPRECATED_WARNING
#endif
// clang-format on

#if defined(__GNUC__) && !defined(__clang__)
#define BENCHMARK_GCC_VERSION (__GNUC__ * 100 + __GNUC_MINOR__)
#endif

#ifndef __has_builtin
#define __has_builtin(x) 0
#endif

#if defined(__GNUC__) || __has_builtin(__builtin_unreachable)
#define BENCHMARK_UNREACHABLE() __builtin_unreachable()
#elif defined(_MSC_VER)
#define BENCHMARK_UNREACHABLE() __assume(false)
#else
#define BENCHMARK_UNREACHABLE() ((void)0)
#endif

#ifdef BENCHMARK_HAS_CXX11
#define BENCHMARK_OVERRIDE override
#else
#define BENCHMARK_OVERRIDE
#endif

#if defined(_MSC_VER)
#pragma warning(push)
// C4251: <symbol> needs to have dll-interface to be used by clients of class
#pragma warning(disable : 4251)
#endif

// The DoNotOptimize(...) function can be used to prevent a value or
// expression from being optimized away by the compiler. This function is
// intended to add little to no overhead.
// See: https://youtu.be/nXaxk27zwlk?t=2441
#ifndef BENCHMARK_HAS_NO_INLINE_ASSEMBLY
#if !defined(__GNUC__) || defined(__llvm__) || defined(__INTEL_COMPILER)
template <class Tp>
BENCHMARK_DEPRECATED_MSG(
    "The const-ref version of this method can permit "
    "undesired compiler optimizations in benchmarks")
inline BENCHMARK_ALWAYS_INLINE void DoNotOptimize(Tp const& value) {
  asm volatile("" : : "r,m"(value) : "memory");
}

template <class Tp>
inline BENCHMARK_ALWAYS_INLINE void DoNotOptimize(Tp& value) {
#if defined(__clang__)
  asm volatile("" : "+r,m"(value) : : "memory");
#else
  asm volatile("" : "+m,r"(value) : : "memory");
#endif
}
#elif defined(BENCHMARK_HAS_CXX11) && (__GNUC__ >= 5)
// Workaround for a bug with full argument copy overhead with GCC.
// See: #1340 and https://gcc.gnu.org/bugzilla/show_bug.cgi?id=105519
template <class Tp>
BENCHMARK_DEPRECATED_MSG(
    "The const-ref version of this method can permit "
    "undesired compiler optimizations in benchmarks")
inline BENCHMARK_ALWAYS_INLINE
    typename std::enable_if<std::is_trivially_copyable<Tp>::value &&
                            (sizeof(Tp) <= sizeof(Tp*))>::type
    DoNotOptimize(Tp const& value) {
  asm volatile("" : : "r,m"(value) : "memory");
}

template <class Tp>
BENCHMARK_DEPRECATED_MSG(
    "The const-ref version of this method can permit "
    "undesired compiler optimizations in benchmarks")
inline BENCHMARK_ALWAYS_INLINE
    typename std::enable_if<!std::is_trivially_copyable<Tp>::value ||
                            (sizeof(Tp) > sizeof(Tp*))>::type
    DoNotOptimize(Tp const& value) {
  asm volatile("" : : "m"(value) : "memory");
}

template <class Tp>
inline BENCHMARK_ALWAYS_INLINE
    typename std::enable_if<std::is_trivially_copyable<Tp>::value &&
                            (sizeof(Tp) <= sizeof(Tp*))>::type
    DoNotOptimize(Tp& value) {
  asm volatile("" : "+m,r"(value) : : "memory");
}

template <class Tp>
inline BENCHMARK_ALWAYS_INLINE
    typename std::enable_if<!std::is_trivially_copyable<Tp>::value ||
                            (sizeof(Tp) > sizeof(Tp*))>::type
    DoNotOptimize(Tp& value) {
  asm volatile("" : "+m"(value) : : "memory");
}

#else
// Fallback for GCC < 5. Can add some overhead because the compiler is forced
// to use memory operations instead of operations with registers.
// TODO: Remove if GCC < 5 will be unsupported.
template <class Tp>
BENCHMARK_DEPRECATED_MSG(
    "The const-ref version of this method can permit "
    "undesired compiler optimizations in benchmarks")
inline BENCHMARK_ALWAYS_INLINE void DoNotOptimize(Tp const& value) {
  asm volatile("" : : "m"(value) : "memory");
}

template <class Tp>
inline BENCHMARK_ALWAYS_INLINE void DoNotOptimize(Tp& value) {
  asm volatile("" : "+m"(value) : : "memory");
}
#endif

#ifndef BENCHMARK_HAS_CXX11
inline BENCHMARK_ALWAYS_INLINE void ClobberMemory() {
  asm volatile("" : : : "memory");
}
#endif
#elif defined(_MSC_VER)
template <class Tp>
BENCHMARK_DEPRECATED_MSG(
    "The const-ref version of this method can permit "
    "undesired compiler optimizations in benchmarks")
inline BENCHMARK_ALWAYS_INLINE void DoNotOptimize(Tp const& value) {
  internal::UseCharPointer(&reinterpret_cast<char const volatile&>(value));
  _ReadWriteBarrier();
}

#ifndef BENCHMARK_HAS_CXX11
inline BENCHMARK_ALWAYS_INLINE void ClobberMemory() { _ReadWriteBarrier(); }
#endif
#else
template <class Tp>
BENCHMARK_DEPRECATED_MSG(
    "The const-ref version of this method can permit "
    "undesired compiler optimizations in benchmarks")
inline BENCHMARK_ALWAYS_INLINE void DoNotOptimize(Tp const& value) {
  internal::UseCharPointer(&reinterpret_cast<char const volatile&>(value));
}
// FIXME Add ClobberMemory() for non-gnu and non-msvc compilers, before C++11.
#endif