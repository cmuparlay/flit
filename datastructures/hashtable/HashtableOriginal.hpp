#ifndef HASHTABLE_ORIGINAL_HPP_
#define HASHTABLE_ORIGINAL_HPP_

#include <vector>
#include <assert.h>

#include <datastructures/harris-linkedlist/ListOriginal.hpp>

// #define DEFAULT_LOAD                    1

template <class T> class alignas(ALIGNMENT) HashtableOriginal{
public:
    
    static const int DEFAULT_SIZE = 1000000;

    HashtableOriginal() {
        num_buckets = DEFAULT_SIZE;
        buckets = new ListOriginal<T>*[num_buckets];
        for (int i = 0; i< num_buckets; i++) {
            buckets[i] = new ListOriginal<T>();
        }
        assert((((uintptr_t) this) % (ALIGNMENT)) == 0); // check alignment
    }
    
    HashtableOriginal(int s) {
        num_buckets = s;
        buckets = new ListOriginal<T>*[num_buckets];
        for (int i = 0; i< num_buckets; i++) {
            buckets[i] = new ListOriginal<T>();
        }
        assert((((uintptr_t) this) % (ALIGNMENT)) == 0); // check alignment
    }

    ~HashtableOriginal() {
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
        return "Hashtable Original";
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
    alignas(128) int num_buckets;
    alignas(128) ListOriginal<T>** buckets;
    
};

#endif /* HASHTABLE_ORIGINAL_HPP_ */
