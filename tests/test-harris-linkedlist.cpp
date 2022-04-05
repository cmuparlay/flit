
#include <assert.h>
#include <vector>
#include <thread>

#include <harris-linkedlist/ListOriginal.hpp>
#include <harris-linkedlist/ListDurableAutomatic.hpp>
#include <harris-linkedlist/ListDurableNvTraverse.hpp>
#include <harris-linkedlist/ListDurableManual.hpp>

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
const int NUM_ITER = 2000;

template<class Set>
void test_simple() {
  Set set;
  assert(!set.contains(2));
  // cout << set.size() << endl;
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

template<class List>
void stress_test() {
  List list;

  vector<int> keys;
  for(int i = 0; i < NUM_THREADS*NUM_ITER; i++)
    keys.push_back(i);
  std::random_shuffle ( keys.begin(), keys.end() );

  Barrier barrier(NUM_THREADS);

  vector<thread> threads;
  for(int p = 0; p < NUM_THREADS; p++) {
    threads.emplace_back([p, &list, &keys, &barrier] () {
      for(int i = p; i < NUM_THREADS*NUM_ITER; i+=NUM_THREADS)
        assert(list.add(keys[i], i));
      barrier.wait();
      for(int i = p; i < NUM_THREADS*NUM_ITER; i+=NUM_THREADS)
        assert(list.remove(i));      
    });        
  }
  for (auto& t : threads) t.join(); 
}

template<class List>
void run_all_tests() {
  test_simple<List>();
  stress_test<List>();  
}

int main() {
  run_all_tests<ListDurableManual<int, persist_counter>>();
  run_all_tests<ListDurableManual<int, persist_simple>>();
  run_all_tests<ListDurableManual<int, persist_hash>>();
  run_all_tests<ListDurableManual<int, persist_hash_cacheline_16>>();
  run_all_tests<ListDurableManual<int, persist_interface>>();
  run_all_tests<ListDurableManual<int, persist_offset_spec>>();
  run_all_tests<ListDurableManual<int, link_and_persist_2>>();

  run_all_tests<ListDurableNvTraverse<int, persist_counter>>();
  run_all_tests<ListDurableNvTraverse<int, persist_simple>>();
  run_all_tests<ListDurableNvTraverse<int, persist_hash>>();
  run_all_tests<ListDurableNvTraverse<int, persist_hash_cacheline_16>>();
  run_all_tests<ListDurableNvTraverse<int, persist_interface>>();
  run_all_tests<ListDurableNvTraverse<int, persist_offset_spec>>();
  run_all_tests<ListDurableNvTraverse<int, link_and_persist_2>>();

  run_all_tests<ListOriginal<int>>();
  run_all_tests<ListDurableAutomatic<int, persist_counter>>();
  run_all_tests<ListDurableAutomatic<int, persist_simple>>();
  run_all_tests<ListDurableAutomatic<int, persist_hash>>();
  run_all_tests<ListDurableAutomatic<int, persist_hash_cacheline_16>>();
  run_all_tests<ListDurableAutomatic<int, persist_interface>>();
  run_all_tests<ListDurableAutomatic<int, persist_offset_spec>>();
  run_all_tests<ListDurableAutomatic<int, link_and_persist_2>>();
  return 0;
}
