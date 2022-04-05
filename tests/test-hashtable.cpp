
#include <assert.h>
#include <vector>
#include <algorithm>
#include <thread>

#include <hashtable/HashtableOriginal.hpp>
#include <hashtable/HashtableDurableAutomatic.hpp>
#include <hashtable/HashtableDurableNvTraverse.hpp>
#include <hashtable/HashtableDurableManual.hpp>

#include <persist/persist.hpp>
#include <persist/persist_counter.hpp>
#include <persist/link_and_persist.hpp>
#include <persist/persist_hash.hpp>
#include <persist/persist_offset.hpp>
#include <persist/persist_hash_cacheline.hpp>
#include <persist/persist_interface.hpp>
#include <persist/persist_simple.hpp>

#include <common/barrier.hpp>

using namespace std;

const int NUM_THREADS = 4;
const int NUM_ITER = 100000;

template<class Set>
void test_simple() {
  Set set;
  assert(!set.contains(2));
  assert(set.size() == 0);
  assert(!set.remove(0));
  assert(set.size() == 0);
  assert(set.add(2, 2));
  assert(set.size() == 1);
  assert(set.add(3, 3));
  assert(set.size() == 2);
  assert(set.add(1, 1));
  assert(set.size() == 3);
  assert(set.keySum() == 6);
  assert(set.contains(2));
  assert(set.contains(3));
  assert(set.contains(1));
  assert(!set.contains(4));
  assert(set.remove(2));
  assert(set.size() == 2);
  assert(!set.contains(2));
  assert(set.contains(3));
  assert(set.contains(1));
  assert(!set.contains(4));
  assert(set.size() == 2);
}

template<class Set>
void stress_test() {
  Set set;
  
  vector<int> keys;
  for(int i = 0; i < NUM_THREADS*NUM_ITER; i++)
    keys.push_back(i);
  std::random_shuffle ( keys.begin(), keys.end() );

  Barrier barrier(NUM_THREADS);

  vector<thread> threads;
  for(int p = 0; p < NUM_THREADS; p++) {
    threads.emplace_back([p, &set, &keys, &barrier] () {
      for(int i = p; i < NUM_THREADS*NUM_ITER; i+=NUM_THREADS)
        assert(set.add(keys[i], i));
      barrier.wait();
      for(int i = p; i < NUM_THREADS*NUM_ITER; i+=NUM_THREADS)
        assert(set.remove(i));      
    });        
  }
  for (auto& t : threads) t.join(); 
}

template<class Set>
void run_all_tests() {
  test_simple<Set>();
  stress_test<Set>();  
}

int main() {
  run_all_tests<HashtableDurableManual<int, persist_counter>>();
  run_all_tests<HashtableDurableManual<int, persist_simple>>();
  run_all_tests<HashtableDurableManual<int, persist_hash>>();
  run_all_tests<HashtableDurableManual<int, persist_hash_cacheline_16>>();
  run_all_tests<HashtableDurableManual<int, persist_interface>>();
  run_all_tests<HashtableDurableManual<int, persist_offset_spec>>();
  run_all_tests<HashtableDurableManual<int, link_and_persist_2>>();

  run_all_tests<HashtableDurableNvTraverse<int, persist_counter>>();
  run_all_tests<HashtableDurableNvTraverse<int, persist_simple>>();
  run_all_tests<HashtableDurableNvTraverse<int, persist_hash>>();
  run_all_tests<HashtableDurableNvTraverse<int, persist_hash_cacheline_16>>();
  run_all_tests<HashtableDurableNvTraverse<int, persist_interface>>();
  run_all_tests<HashtableDurableNvTraverse<int, persist_offset_spec>>();
  run_all_tests<HashtableDurableNvTraverse<int, link_and_persist_2>>();
  
  run_all_tests<HashtableOriginal<int>>();
  run_all_tests<HashtableDurableAutomatic<int, persist_counter>>();
  run_all_tests<HashtableDurableAutomatic<int, persist_simple>>();
  run_all_tests<HashtableDurableAutomatic<int, persist_hash>>();
  run_all_tests<HashtableDurableAutomatic<int, persist_hash_cacheline_16>>();
  run_all_tests<HashtableDurableAutomatic<int, persist_interface>>();
  run_all_tests<HashtableDurableAutomatic<int, persist_offset_spec>>();
  run_all_tests<HashtableDurableAutomatic<int, link_and_persist_2>>();
  return 0;
}
