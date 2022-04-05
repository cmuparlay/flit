
#ifndef PERSIST_INTERFACE_HPP_
#define PERSIST_INTERFACE_HPP_

/* persist_interface<atomic<T>> behaves just like atomic<T>.
   This class is only used to measure the overhead of the interface. */

#include <atomic>
#include "persist.hpp"

// For non-atomic types, use the same implementation as persist<T>
template<typename T, bool DEFAULT_FLUSH_OPTION = flush_option::flush>
struct persist_interface : public persist<T, DEFAULT_FLUSH_OPTION> {};

template<typename T, bool DEFAULT_FLUSH_OPTION>
struct persist_interface<std::atomic<T>, DEFAULT_FLUSH_OPTION> {
  private:
    std::atomic<T> val;

  public:
    persist_interface() noexcept = default;
    ~persist_interface() noexcept = default;
    persist_interface(const persist_interface&) = delete;
    persist_interface& operator=(const persist_interface&) = delete;
    persist_interface& operator=(const persist_interface&) volatile = delete;
  
    persist_interface(T initial, bool flush = DEFAULT_FLUSH_OPTION) noexcept { store_non_atomic(initial, flush); };

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
    }

    // Instead of having an if statenemtn, I can maybe change the
    // memory order to add a fence.
    void store(T newVal, std::memory_order order = std::memory_order_seq_cst,
               bool flush = DEFAULT_FLUSH_OPTION) noexcept {
      // FENCE(); // I believe setting the following memory order 
                  // high enough has the same effect as a fence(). 
                  // TODO: Possible don't need seq_cst.
      val.store(newVal, order);
    }

    T load(std::memory_order order = std::memory_order_seq_cst, 
           bool flush = DEFAULT_FLUSH_OPTION) const noexcept {
      T t = val.load(order); // TODO: if memory order is relaxed,
                             // can the following flush be ordered 
                             // before it?
      return t;
    }

    T exchange(T newVal, 
               std::memory_order order = std::memory_order_seq_cst,
               bool flush = DEFAULT_FLUSH_OPTION) noexcept {
      // seq_cst includes a fence
      T t = val.exchange(newVal, order);
      return t;
    }

    bool compare_exchange_strong(T& oldVal, T newVal,
                 std::memory_order order = std::memory_order_seq_cst,
                 bool flush = DEFAULT_FLUSH_OPTION) noexcept {
      // seq_cst includes a fence
      bool b = val.compare_exchange_strong(oldVal, newVal, 
          order, __cmpexch_failure_order(order));
      return b;
    }

    void flush_if_needed() noexcept {}

    bool is_flush_needed() noexcept {
      return false;
    }

    static std::string get_name() {
      return "persist_interface";
    }
};

#endif /* PERSIST_INTERFACE_HPP_ */
