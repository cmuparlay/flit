
#ifndef SSMEM_WRAPPER_HPP_
#define SSMEM_WRAPPER_HPP_

#include <atomic>

#include "ssmem.h"

struct ssmem_wrapper {
private:
  static std::atomic<int> num_threads;
  ssmem_allocator_t* allocator;

public:
  ssmem_wrapper() {
    int thread_id = num_threads.fetch_add(1);
    allocator = (ssmem_allocator_t*)malloc(sizeof(*allocator));
    ssmem_alloc_init(allocator, SSMEM_DEFAULT_MEM_SIZE, thread_id);
  }

  ~ssmem_wrapper() {
    // ssmem_alloc_term(allocator);
  }

  void* alloc(int size, bool flush = true) {
    return ssmem_alloc(allocator, size, flush);
  }

  void free(void* obj, bool flush = true) {
    ssmem_free(allocator, obj, flush);
  }
};

struct ssmem_destructor {
  ~ssmem_destructor() {
    ssmem_term();
  }
};

std::atomic<int> ssmem_wrapper::num_threads{0};

thread_local ssmem_wrapper ssmem;
ssmem_destructor ssmem_destructor_obj;

#endif /* SSMEM_WRAPPER_HPP_ */