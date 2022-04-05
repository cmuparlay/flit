
#ifndef LINK_AND_PERSIST_HPP_
#define LINK_AND_PERSIST_HPP_

#include <atomic>
#include "persist.hpp"

// TODO: double check this implementation is correct
// TODO: Finish implementing the no flush version of this file

template <class T>
using TO_UINT_T =
  typename std::conditional<(sizeof(T) <= 1), uint8_t,
    typename std::conditional<(sizeof(T) <= 2), uint16_t,
      typename std::conditional<(sizeof(T) <= 4), uint32_t,
        uint64_t
      >::type
    >::type
  >::type;

// For non-atomic types, use the same implementation as persist<T>
// ignores free_bit
template<typename T, int free_bit, bool DEFAULT_FLUSH_OPTION = flush_option::flush>
struct link_and_persist : public persist<T, DEFAULT_FLUSH_OPTION> {};

// uses second bit for link and persist by default
// Assumes T has default constructor
template<typename T, int free_bit, bool DEFAULT_FLUSH_OPTION>
struct link_and_persist<std::atomic<T>, free_bit, DEFAULT_FLUSH_OPTION> {
  private:
    using UINT_T = TO_UINT_T<T>;
    std::atomic<UINT_T> val;

    static const UINT_T MARK_MASK = (1ull << free_bit);
    static const UINT_T MARK_UNMASK = ~(1ull << free_bit);

    static UINT_T set_flush_bit(UINT_T num) {
      return num | MARK_MASK;
    }

    static UINT_T clear_flush_bit(UINT_T num) {
      return num & MARK_UNMASK;
    }

    static bool check_flush_bit(UINT_T num) {
      return num & MARK_MASK;
    }

  public:
    link_and_persist() noexcept { store_non_atomic(T(), DEFAULT_FLUSH_OPTION); };
    ~link_and_persist() noexcept = default;
    link_and_persist(const link_and_persist&) = delete;
    link_and_persist& operator=(const link_and_persist&) = delete;
    link_and_persist& operator=(const link_and_persist&) volatile = delete;
  
    link_and_persist(T initial, bool flush = DEFAULT_FLUSH_OPTION) noexcept { store_non_atomic(initial, flush); };

    operator T() noexcept { return load(); }

    T operator=(T newVal) noexcept { 
      store(newVal); 
      return newVal; 
    }

    bool is_lock_free() const noexcept {
      return val.is_lock_free();
    }

    void load_non_atomic(bool flush = DEFAULT_FLUSH_OPTION) { 
      return reinterpret_cast<T>(clear_flush_bit(val.load(std::memory_order_relaxed))); 
    }

    void store_non_atomic(T newVal, bool flush = DEFAULT_FLUSH_OPTION) {
      val.store(set_flush_bit(reinterpret_cast<UINT_T>(newVal)), std::memory_order_relaxed);
      if (flush == flush_option::flush)
        FLUSH(&val);      
    }

    // This might mess up due to ABA problem if the store races with a CAS
    // I think this is getting complicated enough that we need a proof of correctness
    // The original algorithm was only proposed to be used with CAS.
    // Flush is called regardless of flush_option because the value being overwritten might not have been flushed.
    void store(T newVal, std::memory_order order = std::memory_order_seq_cst, 
               bool flush = DEFAULT_FLUSH_OPTION) noexcept {
      UINT_T newV = reinterpret_cast<UINT_T>(newVal);
      val.store(newV, std::memory_order_seq_cst);
      // if (flush == flush_option::flush) {
      FLUSH(&val);
      val.compare_exchange_strong(newV, set_flush_bit(newV));
      // }
    }

    // If memory order is relaxed, is flush_mark guaranteed to not be set?
    // Izrealivitz could assume this because of his notion of 
    // race freedom.
    T load(std::memory_order order = std::memory_order_seq_cst, 
           bool flush = DEFAULT_FLUSH_OPTION) noexcept {
      UINT_T current = val.load(order);
      if (flush == flush_option::flush) {
        if(!check_flush_bit(current)) {
          FLUSH(&val);
          // UINT_T tmp = current;
          //val.compare_exchange_strong(tmp, set_flush_bit(tmp));
        }
      }
      return reinterpret_cast<T>(clear_flush_bit(current));
    }

    T exchange(T newVal, 
               std::memory_order order = std::memory_order_seq_cst,
               bool flush = DEFAULT_FLUSH_OPTION) noexcept {
      UINT_T newV = reinterpret_cast<UINT_T>(newVal);
      UINT_T current = val.exchange(newV, std::memory_order_seq_cst);
      if (flush == flush_option::flush) {
        FLUSH(&val);      
        val.compare_exchange_strong(newV, set_flush_bit(newV));
      }
      return reinterpret_cast<T>(clear_flush_bit(current));
    }

    bool compare_exchange_strong(T& oldVal, T newVal,
                std::memory_order order = std::memory_order_seq_cst, 
                bool flush = DEFAULT_FLUSH_OPTION) noexcept {
      if (flush == flush_option::flush) {
        UINT_T current = val.load(order);
        // TODO: This while loop can maybe be avoided
        while(clear_flush_bit(current) == reinterpret_cast<UINT_T>(oldVal)) {
          if(val.compare_exchange_strong(current, reinterpret_cast<UINT_T>(newVal), order)) {
            FLUSH(&val);
            UINT_T newV = reinterpret_cast<UINT_T>(newVal);
            val.compare_exchange_strong(newV, set_flush_bit(newV));
            return true;
          }
        }
        oldVal = reinterpret_cast<T>(clear_flush_bit(current));
        if(!check_flush_bit(current)) {
          FLUSH(&val);
          val.compare_exchange_strong(current, set_flush_bit(current));
        }
        return false; 
      } else {
        UINT_T current = val.load(order);
        // TODO: This while loop can maybe be avoided
        while(clear_flush_bit(current) == reinterpret_cast<UINT_T>(oldVal)) {
          if(val.compare_exchange_strong(current, reinterpret_cast<UINT_T>(newVal), order)) {
            return true;
          }
        }
        oldVal = reinterpret_cast<T>(clear_flush_bit(current));
        return false; 
      }
    }

    void flush_if_needed() noexcept {
      UINT_T current = val.load(std::memory_order_relaxed);
      // I think the previous load can use relaxed memory order because
      // flushes are not ordered until the next fence anyways.
      if(!check_flush_bit(current)) {
        FLUSH(&val);
        // UINT_T tmp = current;
        //val.compare_exchange_strong(tmp, set_flush_bit(tmp));
      }
    }

    bool is_flush_needed() noexcept {
      UINT_T current = val.load(std::memory_order_relaxed);
      return !check_flush_bit(current);
    }

    static std::string get_name() {
      return "link_and_persist";
    }
};

template <typename T, bool DEFAULT_FLUSH_OPTION = flush_option::flush>
using link_and_persist_1 = link_and_persist<T, 1, DEFAULT_FLUSH_OPTION>;

template <typename T, bool DEFAULT_FLUSH_OPTION = flush_option::flush>
using link_and_persist_2 = link_and_persist<T, 2, DEFAULT_FLUSH_OPTION>;

#endif /* LINK_AND_PERSIST_HPP_ */
