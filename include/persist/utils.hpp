
#ifndef UTILS_HPP_
#define UTILS_HPP_

namespace utils {

  // a slightly cheaper, but possibly not as good version
  // based on splitmix64
  inline uint64_t hash64(uint64_t x) {
    x = (x ^ (x >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
    x = (x ^ (x >> 27)) * UINT64_C(0x94d049bb133111eb);
    x = x ^ (x >> 31);
    return x;
  }

  inline int round_up(int x, int alignment) {
    if(x % alignment == 0) return x;
    else return (x/alignment+1)*alignment;
  }

  const int LOG_CACHE_LINE_SIZE = 6;
  const int LOG_NUM_FLUSH_COUNTERS = 16;
  const uint64_t NUM_FLUSH_COUNTERS = (1ull<<LOG_NUM_FLUSH_COUNTERS);
  const uint64_t FLUSH_COUNTER_MASK = NUM_FLUSH_COUNTERS-1ull;
  const uint64_t CACHE_LINE_MASK = ~((1ull<<LOG_CACHE_LINE_SIZE)-1ull);
}

#endif /* UTILS_HPP_ */