
#ifndef PERSIST_HASH_HPP_
#define PERSIST_HASH_HPP_

#include<atomic>
#include "persist.hpp"
#include "utils.hpp"

// Flush marking can support fetch and add and other common atomic primitives.

// For non-atomic types, use the same implementation as persist<T>
template<typename T, bool DEFAULT_FLUSH_OPTION = flush_option::flush>
struct persist_hash : public persist<T, DEFAULT_FLUSH_OPTION> {};

template<typename T, bool DEFAULT_FLUSH_OPTION>
struct persist_hash<std::atomic<T>, DEFAULT_FLUSH_OPTION> {
  private:
    std::atomic<T> val;

    static std::atomic<uint8_t> flush_counters[utils::NUM_FLUSH_COUNTERS];

    static int get_index(const std::atomic<T>* ptr) {
      return utils::hash64((uint64_t) ptr) & utils::FLUSH_COUNTER_MASK;
    }

  public:
    persist_hash() noexcept = default;
    ~persist_hash() noexcept = default;
    persist_hash(const persist_hash&) = delete;
    persist_hash& operator=(const persist_hash&) = delete;
    persist_hash& operator=(const persist_hash&) volatile = delete;
  
    persist_hash(T initial, bool flush = DEFAULT_FLUSH_OPTION) noexcept { store_non_atomic(initial, flush); };

    operator T() const noexcept { return load(); }

    T operator=(T newVal) noexcept { 
      store(newVal); 
      return newVal; 
    }

    bool is_lock_free() const noexcept {
      return val.is_lock_free();
    }

    void load_non_atomic(bool flush = DEFAULT_FLUSH_OPTION) { return val.load(std::memory_order_relaxed); }

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
           bool flush = DEFAULT_FLUSH_OPTION) 
                const noexcept {
      T t = val.load(order);
      if (flush == flush_option::flush)
        if(flush_counters[get_index(&val)]) FLUSH(&val);
      return t;
    }

    T exchange(T newVal, 
               std::memory_order order = std::memory_order_seq_cst,
               bool flush = DEFAULT_FLUSH_OPTION) noexcept {
      if (flush == flush_option::flush) {
        int flush_counter_index = get_index(&val);
        flush_counters[flush_counter_index].fetch_add(1);
        T t = val.exchange(newVal, order);
        FLUSH(&val);      
        flush_counters[flush_counter_index].fetch_sub(1);
        return t;
      }
      else val.exchange(newVal, order);
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
      return "persist_hash";
    }
};

template<typename T, bool DEFAULT_FLUSH_OPTION> 
std::atomic<uint8_t> persist_hash<std::atomic<T>, DEFAULT_FLUSH_OPTION>::flush_counters[utils::NUM_FLUSH_COUNTERS];

#endif /* PERSIST_HASH_HPP_ */