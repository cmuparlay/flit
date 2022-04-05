#ifndef HASHTABLE_DURABLE_AUTOMATIC_HPP_
#define HASHTABLE_DURABLE_AUTOMATIC_HPP_

#include <vector>
#include <assert.h>

#include <datastructures/harris-linkedlist/ListDurableAutomatic.hpp>
#include <persist/persist.hpp>

template <class T, template<typename, bool> typename PERSIST> 
class alignas(ALIGNMENT) HashtableDurableAutomatic{
public:
    
    static const int DEFAULT_SIZE = 100000;

    HashtableDurableAutomatic() : HashtableDurableAutomatic(DEFAULT_SIZE) {
        assert((((uintptr_t) this) % (ALIGNMENT)) == 0); // check alignment
    }
    
    HashtableDurableAutomatic(int s) : 
                       num_buckets(s), 
                       buckets(new persist<ListDurableAutomatic<T, PERSIST>*>[num_buckets]) {
        for (int i = 0; i< num_buckets; i++) {
            buckets[i] = new ListDurableAutomatic<T, PERSIST>();
        }
        assert((((uintptr_t) this) % (ALIGNMENT)) == 0); // check alignment
    }
    
    ~HashtableDurableAutomatic() {
        for (int i = 0; i< num_buckets; i++) {
            delete buckets[i];
        }
        delete[] buckets;
    }

    bool add(int k, T item) {
        //std:: cout << "k: " << k << std::endl;
        int index = k%num_buckets;
        //iistd:: cout << "index: " << index << std::endl;
        
        bool b = buckets[index]->add(k, item);
        //std:: cout << "res: " << b << std::endl;
        
        return b;
    }
    
    bool remove(int k) {
        int index = k%num_buckets;
        bool b = buckets[index]->remove(k);
        return b;
    }
    
    bool contains(int k) {
        int index = k%num_buckets;
        bool b = buckets[index]->contains(k);
        return b;
    }

    static std::string get_name() {
        return "Hashtable Auto, " + PERSIST<std::atomic<int>, flush_option::flush>::get_name();
    }

    //========================================
    
    /* Functions for debugging and validation.
       Must be run in a quiescent state.
     */

    long long size() {
      long long s = 0;
      for(int i = 0; i < num_buckets; i++)
        s += buckets[i]->size();
      return s;
    }

    long long keySum() {
      long long s = 0;
      for(int i = 0; i < num_buckets; i++)
        s += buckets[i]->keySum();
      return s;
    }

private:
    alignas(128) persist<int> num_buckets;
    alignas(128) persist<persist<ListDurableAutomatic<T, PERSIST>*>*> buckets;
};

#endif /* HASHTABLE_DURABLE_AUTOMATIC_HPP_ */
