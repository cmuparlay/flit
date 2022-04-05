#ifndef PMEM_UTILS_HPP_
#define PMEM_UTILS_HPP_

//TOOD:
// Count flushes for GC separately
// figure out if flush, fence, and CAS counts make sense
// there was an optimization we did not apply to izrealivitz which pushes their fence count very high.
// On a machine without CLFLUSHOPT or CLWB, it doesn't make sense to count fences because they are no-ops
// Rachid's code should be

#include <iostream>
#include <atomic>
// #include <csignal>

// #define PWB_IS_CLFLUSH

#define MFENCE __sync_synchronize
#define SAME_CACHELINE(a, b) ((((uint64_t)(a))>>6) == (((uint64_t)(b))>>6))
const uint64_t CACHELINE_MASK = ~(64ULL-1ULL);

thread_local bool disable_flushes = false;

#ifdef PMEM_STATS
  std::atomic<int64_t> global_flush_count(0);
  std::atomic<int64_t> global_fence_count(0);
  std::atomic<int64_t> global_cas_count(0);
  
  thread_local int64_t flush_count = 0;
  thread_local int64_t fence_count = 0;
  thread_local int64_t cas_count = 0;

  void reset_pmem_stats() {
    flush_count = fence_count = cas_count = 0;
    global_flush_count = global_fence_count = global_cas_count = 0;
  }

  void aggregate_pmem_stats() {
    global_flush_count += flush_count;
    global_fence_count += fence_count;
    global_cas_count += cas_count;
  }

  void print_pmem_stats() {
    std::cout << "Flush count: " << global_flush_count << std::endl;
    std::cout << "Fence count: " << global_fence_count << std::endl;
    std::cout << "CAS count: " << global_cas_count << std::endl;
  }

  void print_pmem_stats(uint64_t num_operations) {
    std::cout << "Flushes per operation: " << 1.0*global_flush_count/num_operations << std::endl;
    std::cout << "Fences per operation: " << 1.0*global_fence_count/num_operations << std::endl;
    std::cout << "CASes per operation: " << 1.0*global_cas_count/num_operations << std::endl;
  }
#endif

template <class ET>
inline void FLUSH(ET *p)
{
  if(disable_flushes) return;
  #ifdef PMEM_STATS
    flush_count++;
  #endif
  // std::raise(SIGINT);

  #ifdef PWB_IS_CLFLUSH
    asm volatile ("clflush (%0)" :: "r"(p));
  #elif PWB_IS_CLFLUSHOPT
      asm volatile(".byte 0x66; clflush %0" : "+m" (*(volatile char *)(p)));    // clflushopt (Kaby Lake)
  #elif PWB_IS_CLWB
      asm volatile(".byte 0x66; xsaveopt %0" : "+m" (*(volatile char *)(p)));  // clwb() only for Ice Lake onwards
  #else
  #error "You must define what PWB is. Choose PWB_IS_CLFLUSH if you don't know what your CPU is capable of"
  #endif
}

// assumes that ptr + size will not go out of the struct
// also assumes that structs fit in one cache line when aligned
template <class ET>
inline void FLUSH_STRUCT(ET *ptr, size_t size)
{
  #if defined(CACHE_ALIGN)
    FLUSH(ptr);
  #else
    //cout << "FLUSH_STRUCT(" << (uint64_t) ptr << " " << size << ")" << endl;
    for(uint64_t p = ((uint64_t) ptr)&CACHELINE_MASK; p < ((uint64_t) ptr) + size; p += 64ULL)
      //cout << p << endl;
      FLUSH((void*) p);
	#endif
}  

template <class ET>
inline void FLUSH_STRUCT(ET *ptr)
{
  #if defined(CACHE_ALIGN)
    FLUSH(ptr);
  #else
    FLUSH_STRUCT(ptr, sizeof(ET));
  #endif
  //for(char *p = (char *) ptr; (uint64_t) p < (uint64_t) (ptr+1); p += 64)
  //  FLUSH(p);
}  

// flush word pointed to by ptr in node n
template <class ET, class NODE_T>
inline void FLUSH_node(ET *ptr, NODE_T *n)
{
  //if(!SAME_CACHELINE(ptr, n))
  //  std::cerr << "FLUSH NOT ON SAME_CACHELINE" << std::endl;
  #ifdef MARK_FLUSHED
    if(n->flushed)
      FLUSH(ptr);
  #else
    FLUSH(ptr);
  #endif
}

// flush entire node pointed to by ptr
template <class ET>
inline void FLUSH_node(ET *ptr)
{
  #ifdef MARK_FLUSHED
    if(ptr->flushed)
      FLUSH_STRUCT(ptr);
  #else
    FLUSH_STRUCT(ptr);
  #endif
}

inline void FENCE()
{
  if(disable_flushes) return;
  #ifdef PWB_IS_CLFLUSH
    //MFENCE();
  #elif PWB_IS_CLFLUSHOPT
    asm volatile ("sfence" ::: "memory");
    #ifdef PMEM_STATS
      fence_count++;
    #endif
  #elif PWB_IS_CLWB
    asm volatile ("sfence" ::: "memory");
    #ifdef PMEM_STATS
      fence_count++;
    #endif
  #else
    #error "You must define what PWB is. Choose PWB_IS_CLFLUSH if you don't know what your CPU is capable of"
  #endif
}

inline std::string get_flush_instruction() {
  #ifdef PWB_IS_CLFLUSH
    return "CLFLUSH";
  #elif PWB_IS_CLFLUSHOPT
    return "CLFLUSHOPT";
  #elif PWB_IS_CLWB
    return "CLWB";
  #else
    return "Flush Instruction Undefined";
  #endif
}

#endif /* PMEM_UTILS_HPP_ */
