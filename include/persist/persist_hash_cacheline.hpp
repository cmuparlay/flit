
#ifndef PERSIST_HASH_CACHELINE_HPP_
#define PERSIST_HASH_CACHELINE_HPP_

#include <atomic>
#include "persist.hpp"
#include "utils.hpp"
#include <sstream>

// Flush marking can support fetch and add and other common atomic primitives.

#define LOG_CACHE_LINE_SIZE 6
const uint64_t CACHE_LINE_MASK = (1ull<<LOG_CACHE_LINE_SIZE)-1ull;

// For non-atomic types, use the same implementation as persist<T>
template<typename T, int LOG_NUM_FLUSH_COUNTERS, bool DEFAULT_FLUSH_OPTION = flush_option::flush>
struct persist_hash_cacheline : public persist<T, DEFAULT_FLUSH_OPTION> {};

template<typename T, int LOG_NUM_FLUSH_COUNTERS, bool DEFAULT_FLUSH_OPTION>
struct persist_hash_cacheline<std::atomic<T>, LOG_NUM_FLUSH_COUNTERS, DEFAULT_FLUSH_OPTION> {
  private:
    std::atomic<T> val;

    static const uint64_t NUM_FLUSH_COUNTERS = (1ull<<LOG_NUM_FLUSH_COUNTERS);
    static const uint64_t FLUSH_COUNTER_MASK = NUM_FLUSH_COUNTERS-1ull;
    static std::atomic<uint8_t> flush_counters[NUM_FLUSH_COUNTERS];

    static int get_index(const std::atomic<T>* ptr) {
      return utils::hash64(((uint64_t) ptr) & utils::CACHE_LINE_MASK) & FLUSH_COUNTER_MASK;
    }

  public:
    persist_hash_cacheline() noexcept = default;
    ~persist_hash_cacheline() noexcept = default;
    persist_hash_cacheline(const persist_hash_cacheline&) = delete;
    persist_hash_cacheline& operator=(const persist_hash_cacheline&) = delete;
    persist_hash_cacheline& operator=(const persist_hash_cacheline&) volatile = delete;
  
    persist_hash_cacheline(T initial, bool flush = DEFAULT_FLUSH_OPTION) noexcept { store_non_atomic(initial, flush); };

    operator T() const noexcept { return load(); }

    T operator=(T newVal) noexcept { 
      store(newVal); 
      return newVal; 
    }

    bool is_lock_free() const noexcept {
      return val.is_lock_free();
    }

    void load_non_atomic(bool flush = DEFAULT_FLUSH_OPTION) { 
      return val.load(std::memory_order_relaxed); 
    }

    void store_non_atomic(T newVal, bool flush = DEFAULT_FLUSH_OPTION) {
      val.store(newVal, std::memory_order_relaxed);
      if (flush == flush_option::flush)
        FLUSH(&val);      
    }

    void store(T newVal, std::memory_order order = std::memory_order_seq_cst,
               bool flush = DEFAULT_FLUSH_OPTION) noexcept {
      if (flush == flush_option::flush) {
        int flush_counter_index = get_index(&val);
        flush_counters[flush_counter_index].fetch_add(1);
        val.store(newVal, order);
        FLUSH(&val);      
        flush_counters[flush_counter_index].fetch_sub(1);
      }
      else val.store(newVal, order);
    }

    T load(std::memory_order order = std::memory_order_seq_cst, 
           bool flush = DEFAULT_FLUSH_OPTION) const noexcept {
      T t = val.load(order);
      if (flush == flush_option::flush)
        if(flush_counters[get_index(&val)]) FLUSH(&val);
      return t;
    }

    T exchange(T newVal, std::memory_order order = std::memory_order_seq_cst, 
               bool flush = DEFAULT_FLUSH_OPTION) noexcept {
      if (flush == flush_option::flush) {
        int flush_counter_index = get_index(&val);
        flush_counters[flush_counter_index].fetch_add(1);
        T t = val.exchange(newVal, order);
        FLUSH(&val);      
        flush_counters[flush_counter_index].fetch_sub(1);
        return t;
      }
      else return val.exchange(newVal, order);
    }

    bool compare_exchange_strong(T& oldVal, T newVal,
              std::memory_order order = std::memory_order_seq_cst, 
              bool flush = DEFAULT_FLUSH_OPTION) noexcept {
      if (flush == flush_option::flush) {
        int flush_counter_index = get_index(&val);
        flush_counters[flush_counter_index].fetch_add(1);
        bool b = val.compare_exchange_strong(oldVal, newVal, order, 
                                     __cmpexch_failure_order(order));
        FLUSH(&val);      
        flush_counters[flush_counter_index].fetch_sub(1);
        return b;
      }
      else return val.compare_exchange_strong(oldVal, newVal, order, 
                                   __cmpexch_failure_order(order));
    }

    void flush_if_needed() noexcept {
      if(flush_counters[get_index(&val)]) FLUSH(&val);
    }

    bool is_flush_needed() noexcept {
      return flush_counters[get_index(&val)];
    }

    static std::string get_name() {
      std::stringstream ss;
      ss << "persist_hash_" << LOG_NUM_FLUSH_COUNTERS;
      return ss.str();
    }
};

template<typename T, int LOG_NUM_FLUSH_COUNTERS, bool DEFAULT_FLUSH_OPTION>
std::atomic<uint8_t> persist_hash_cacheline<std::atomic<T>, LOG_NUM_FLUSH_COUNTERS, DEFAULT_FLUSH_OPTION>::flush_counters[persist_hash_cacheline<std::atomic<T>, LOG_NUM_FLUSH_COUNTERS, DEFAULT_FLUSH_OPTION>::NUM_FLUSH_COUNTERS];

// template<typename T, bool DEFAULT_FLUSH_OPTION = flush_option::flush>
// using persist_hash_cacheline_10 = persist_hash_cacheline<T, 10, DEFAULT_FLUSH_OPTION>;

// template<typename T, bool DEFAULT_FLUSH_OPTION = flush_option::flush>
// using persist_hash_cacheline_11 = persist_hash_cacheline<T, 11, DEFAULT_FLUSH_OPTION>;

template<typename T, bool DEFAULT_FLUSH_OPTION = flush_option::flush>
using persist_hash_cacheline_12 = persist_hash_cacheline<T, 12, DEFAULT_FLUSH_OPTION>;

// template<typename T, bool DEFAULT_FLUSH_OPTION = flush_option::flush>
// using persist_hash_cacheline_13 = persist_hash_cacheline<T, 13, DEFAULT_FLUSH_OPTION>;

// template<typename T, bool DEFAULT_FLUSH_OPTION = flush_option::flush>
// using persist_hash_cacheline_14 = persist_hash_cacheline<T, 14, DEFAULT_FLUSH_OPTION>;

// template<typename T, bool DEFAULT_FLUSH_OPTION = flush_option::flush>
// using persist_hash_cacheline_15 = persist_hash_cacheline<T, 15, DEFAULT_FLUSH_OPTION>;

template<typename T, bool DEFAULT_FLUSH_OPTION = flush_option::flush>
using persist_hash_cacheline_16 = persist_hash_cacheline<T, 16, DEFAULT_FLUSH_OPTION>;

// template<typename T, bool DEFAULT_FLUSH_OPTION = flush_option::flush>
// using persist_hash_cacheline_17 = persist_hash_cacheline<T, 17, DEFAULT_FLUSH_OPTION>;

// template<typename T, bool DEFAULT_FLUSH_OPTION = flush_option::flush>
// using persist_hash_cacheline_18 = persist_hash_cacheline<T, 18, DEFAULT_FLUSH_OPTION>;

// template<typename T, bool DEFAULT_FLUSH_OPTION = flush_option::flush>
// using persist_hash_cacheline_19 = persist_hash_cacheline<T, 19, DEFAULT_FLUSH_OPTION>;

template<typename T, bool DEFAULT_FLUSH_OPTION = flush_option::flush>
using persist_hash_cacheline_20 = persist_hash_cacheline<T, 20, DEFAULT_FLUSH_OPTION>;

template<typename T, bool DEFAULT_FLUSH_OPTION = flush_option::flush>
using persist_hash_cacheline_23 = persist_hash_cacheline<T, 23, DEFAULT_FLUSH_OPTION>;

template<typename T, bool DEFAULT_FLUSH_OPTION = flush_option::flush>
using persist_hash_cacheline_26 = persist_hash_cacheline<T, 26, DEFAULT_FLUSH_OPTION>;

#endif /* PERSIST_HASH_CACHELINE_HPP_ */
