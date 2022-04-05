
#ifndef BENCHMARKS_COMMON_HPP_
#define BENCHMARKS_COMMON_HPP_

#include <atomic>
#include <chrono>
#include <iostream>

// ==================================================================
//                     Benchmarking framework
// ==================================================================

// Generic base class for benchmarks
// Benchmarks should implement the bench() function, which
// runs the benchmark with P threads.
struct Benchmark {
  
  Benchmark() { }

  void start_timer() {
    start = std::chrono::high_resolution_clock::now();
  }

  double read_timer() {
    auto now = std::chrono::high_resolution_clock::now();
    double elapsed_seconds = std::chrono::duration<double>(now - start).count();
    return elapsed_seconds;
  }

  virtual void bench() = 0;
  virtual ~Benchmark() = default;
  
  std::chrono::time_point<std::chrono::high_resolution_clock> start;
  //int P;    // Number of threads
};

#endif  // BENCHMARKS_COMMON_HPP_

