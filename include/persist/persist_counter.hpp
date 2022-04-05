
#ifndef PERSIST_COUNTER_HPP_
#define PERSIST_COUNTER_HPP_

#include <atomic>
#include "persist.hpp"

// For non-atomic types, use the same implementation as persist<T>
template<typename T, bool DEFAULT_FLUSH_OPTION = flush_option::flush>
struct persist_counter : public persist<T, DEFAULT_FLUSH_OPTION> {};

template<typename T, bool DEFAULT_FLUSH_OPTION>
struct persist_counter<std::atomic<T>, DEFAULT_FLUSH_OPTION> {
  private:
    std::atomic<T> val;
    std::atomic<uint8_t> flush_counter;

  public:
    persist_counter() noexcept : val(), flush_counter(0) {};
    ~persist_counter() noexcept = default;
    persist_counter(const persist_counter&) = delete;
    persist_counter& operator=(const persist_counter&) = delete;
    persist_counter& operator=(const persist_counter&) volatile = delete;
  
    persist_counter(T initial, bool flush = DEFAULT_FLUSH_OPTION) noexcept : flush_counter(0) { store_non_atomic(initial, flush); };

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

    T load(std::memory_order order = std::memory_order_seq_cst, 
           bool flush = DEFAULT_FLUSH_OPTION) const noexcept {
      T t = val.load(order);
      if (flush == flush_option::flush)
        if(flush_counter) FLUSH(&val);
      return t;
    }    

    // TODO: see if the memory order on fetch_add can be weakened
    void store(T newVal, std::memory_order order = std::memory_order_seq_cst,
               bool flush = DEFAULT_FLUSH_OPTION) noexcept {
      if (flush == flush_option::flush) {
        flush_counter.fetch_add(1);
        val.store(newVal, order);
        FLUSH(&val);      
        flush_counter.fetch_sub(1);      
      }
      else val.store(newVal, order);
    }

    T exchange(T newVal, std::memory_order order = std::memory_order_seq_cst,
               bool flush = DEFAULT_FLUSH_OPTION) noexcept {
      if (flush == flush_option::flush) {
        flush_counter.fetch_add(1);
        T t = val.exchange(newVal, order);
        FLUSH(&val);      
        flush_counter.fetch_sub(1);
        return t;
      }
      else return val.exchange(newVal, order);
    }

    bool compare_exchange_strong(T& oldVal, T newVal,
                std::memory_order order = std::memory_order_seq_cst,
                bool flush = DEFAULT_FLUSH_OPTION) noexcept {
      if (flush == flush_option::flush) {
        flush_counter.fetch_add(1);
        bool b = val.compare_exchange_strong(oldVal, newVal, order, 
                                       __cmpexch_failure_order(order));
        FLUSH(&val);      // TODO: This flush can some times be avoided on failure.
        flush_counter.fetch_sub(1);
        return b;
      }
      else return val.compare_exchange_strong(oldVal, newVal, order, 
                                       __cmpexch_failure_order(order));
    }

    void flush_if_needed() noexcept {
      if(flush_counter) FLUSH(&val);
    }

    bool is_flush_needed() noexcept {
      return flush_counter;
    }

    static std::string get_name() {
      return "persist_counter";
    }
};

#endif /* PERSIST_COUNTER_HPP_ */