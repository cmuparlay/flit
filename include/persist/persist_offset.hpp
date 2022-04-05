
#ifndef PERSIST_OFFSET_HPP_
#define PERSIST_OFFSET_HPP_

#include<atomic>
#include "persist.hpp"
#include "utils.hpp"

using flush_counter_t = std::atomic<uint8_t>;

// For non-atomic types, use the same implementation as persist<T>
template<typename T, int flush_counter_offset, bool DEFAULT_FLUSH_OPTION = flush_option::flush>
struct persist_offset : public persist<T, DEFAULT_FLUSH_OPTION> {};

template<typename T, int flush_counter_offset, bool DEFAULT_FLUSH_OPTION>
struct persist_offset<std::atomic<T>, flush_counter_offset,
                                      DEFAULT_FLUSH_OPTION> {
  private:
    std::atomic<T> val;

    flush_counter_t* get_flush_counter() const {
      return (flush_counter_t*) (((uint64_t) this) + flush_counter_offset);
    }

  public:
    persist_offset() noexcept = default;
    ~persist_offset() noexcept = default;
    persist_offset(const persist_offset&) = delete;
    persist_offset& operator=(const persist_offset&) = delete;
    persist_offset& operator=(const persist_offset&) volatile = delete;
  
    persist_offset(T initial, bool flush = DEFAULT_FLUSH_OPTION) noexcept { store_non_atomic(initial, flush); };

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
        get_flush_counter()->fetch_add(1);
        val.store(newVal, order);
        FLUSH(&val);      
        get_flush_counter()->fetch_sub(1);
      }
      else val.store(newVal, order);
    }

    T load(std::memory_order order = std::memory_order_seq_cst,
           bool flush = DEFAULT_FLUSH_OPTION) const noexcept {
      T t = val.load(order);
      if (flush == flush_option::flush)
        if(get_flush_counter()->load()) FLUSH(&val);
      return t;
    }

    T exchange(T newVal, 
               std::memory_order order = std::memory_order_seq_cst,
               bool flush = DEFAULT_FLUSH_OPTION) noexcept {
      if (flush == flush_option::flush) {
        get_flush_counter()->fetch_add(1);
        T t = val.exchange(newVal, order);
        FLUSH(&val);      
        get_flush_counter()->fetch_sub(1);
        return t;
      }
      else return val.exchange(newVal, order);
    }

    bool compare_exchange_strong(T& oldVal, T newVal,
                std::memory_order order = std::memory_order_seq_cst,
                bool flush = DEFAULT_FLUSH_OPTION) noexcept {
      if (flush == flush_option::flush) {
        get_flush_counter()->fetch_add(1);
        bool b = val.compare_exchange_strong(oldVal, newVal, order, 
                                     __cmpexch_failure_order(order));
        FLUSH(&val);      
        get_flush_counter()->fetch_sub(1);
        return b;
      }
      else return val.compare_exchange_strong(oldVal, newVal, order, 
                                     __cmpexch_failure_order(order));
    }

    void flush_if_needed() noexcept {
      if(get_flush_counter()->load()) FLUSH(&val);
    }

    bool is_flush_needed() noexcept {
      return get_flush_counter()->load();
    }

    static std::string get_name() {
      return "persist_offset";
    }
};

// persist_offset_spec is only used for template specialization
template <typename T, bool B> struct persist_offset_spec {
  static std::string get_name() {
    return "persist_offset";
  }
};

#endif /* PERSIST_OFFSET_HPP_ */