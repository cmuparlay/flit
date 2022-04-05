
#include <algorithm>
#include <chrono>
#include <iostream>
#include <numeric>
#include <vector>
#include <stdlib.h>
#include <string>

#include <boost/program_options.hpp>

#include <common/rand_r_32.h>
#include <common/barrier.hpp>

#include <harris-linkedlist/ListOriginal.hpp>
#include <harris-linkedlist/ListDurableAutomatic.hpp>
#include <harris-linkedlist/ListDurableNvTraverse.hpp>
#include <harris-linkedlist/ListDurableManual.hpp>

#include <aravind-bst/AravindBstOriginal.hpp>
#include <aravind-bst/AravindBstDurableAutomatic.hpp>
#include <aravind-bst/AravindBstDurableNvTraverse.hpp>
#include <aravind-bst/AravindBstDurableManual.hpp>

#include <hashtable/HashtableOriginal.hpp>
#include <hashtable/HashtableDurableAutomatic.hpp>
#include <hashtable/HashtableDurableNvTraverse.hpp>
#include <hashtable/HashtableDurableManual.hpp>

#include <skiplist/SkiplistOriginal.hpp>
#include <skiplist/SkiplistDurableAutomatic.hpp>
#include <skiplist/SkiplistDurableNvTraverse.hpp>
#include <skiplist/SkiplistDurableManual.hpp>

#include <persist/persist.hpp>
#include <persist/persist_counter.hpp>
#include <persist/link_and_persist.hpp>
#include <persist/persist_hash.hpp>
#include <persist/persist_offset.hpp>
#include <persist/persist_hash_cacheline.hpp>
#include <persist/persist_interface.hpp>
#include <persist/persist_simple.hpp>

#include "common.hpp"

using namespace std;
namespace po = boost::program_options;

/* Set is a datastructure where the keys and values are both integers */
template<class Set>
struct FixedSizeBenchmark : Benchmark {

  FixedSizeBenchmark(int _thread_count, int _size, int _update_percent, double _runtime): 
                     Benchmark(), set(_size), thread_count(_thread_count), size(_size), range(2*_size), 
                     update_percent(_update_percent),
                     processor_count(std::thread::hardware_concurrency()/2), runtime(_runtime) {
			    
    cout << "Detected " << processor_count << " hardware threads, " << ALIGNMENT << "byte alignment" << endl;
    if(processor_count == 0) {
      cerr << "unable to determine hardware processor count" << endl;
      exit(1);
    }
  }

  void bench() override {
    std::vector<long long int> ops(processor_count);
    std::vector<long long int> numKeys(processor_count);
    std::vector<long long int> keySum(processor_count);
    std::vector<std::thread> threads;

    std::atomic<bool> start = false;
    std::atomic<bool> done = false;
    bool parallel_init = false;

    Barrier barrier(processor_count+1);

    for (int p = 0; p < processor_count; p++) {
      threads.emplace_back([&barrier, &start, &done, this, &ops, &numKeys, &keySum, p, parallel_init]() {
        // disable_flushes = true;
        ssmem.alloc(ALIGNMENT); // for ssmem to initialize
        my_rand::init(p);
        long long int localNumKeys = 0;
        long long int localkeySum = 0;
        long long int localOps = 0;
        // prefill
        if(parallel_init) {
          for(int i = p; i < size;) {
            int key = my_rand::get_rand()%range;
            if(set.add(key, range + key)) {
  	          i += processor_count;
              localNumKeys++;
              localkeySum += key;
            }
          }
        } else if(p == 0) {
          for(int i = p; i < size;) {
            int key = my_rand::get_rand()%range;
            if(set.add(key, range + key)) {
              i += 1;
              localNumKeys++;
              localkeySum += key;
            }
          }
        }
        
        #ifdef PMEM_STATS
          reset_pmem_stats();
        #endif

        // Wait for everyone to finish initializing

        barrier.wait();
        while(!start);

        // Only run benchmark with thread_count threads
        if(p < thread_count) { 
          volatile long long int dummy = 0;
          for (; !done; localOps++) {
            int add_or_remove = my_rand::get_rand()%2;

            int update_or_lookup = my_rand::get_rand()%100;
            int key = my_rand::get_rand()%range;
            //cout << asp_index << endl;
            if(update_or_lookup < update_percent) { // update
              if(add_or_remove == 0) { // add
                if(set.add(key, range+key)) {
                  localNumKeys++;
                  localkeySum += key;
                }
              } else {                // remove
                if(set.remove(key)) {
                  localNumKeys--;
                  localkeySum -= key;
                }
              }
            } else {  // contains
                if(set.contains(key)){
                    dummy += 1; // to prevent contains from being optimized out
                }
	          }
          }   
        }
        ops[p] = localOps;   
        numKeys[p] = localNumKeys;
        keySum[p] = localkeySum;  
        #ifdef PMEM_STATS
          aggregate_pmem_stats();
        #endif
      });
    }
    
    // Wait for data structure to be prefilled
    barrier.wait();

    // Check initial size of data structure
    assert(set.size() == size);

    // Run benchmark for fixed amount of time
    start_timer();
    start = true;
    usleep(runtime*1000000);
    // double elapsed_seconds = 0;
    // while (elapsed_seconds < runtime) { 
    //   elapsed_seconds = read_timer();
    // }
    done = true;
    double elapsed_seconds = read_timer();
    
    for (auto& t : threads) t.join();

    // Accumulate results
    long long int totalOps = std::accumulate(std::begin(ops), std::end(ops), 0LL);
    long long int totalKeySum = std::accumulate(std::begin(keySum), std::end(keySum), 0LL);
    long long int totalNumKeys = std::accumulate(std::begin(numKeys), std::end(numKeys), 0LL);
    // long long int actualNumKeys = set.size();
    long long int actualKeySum = set.keySum();
    cout << "Size after benchmark: " << totalNumKeys << endl;
    if(totalKeySum == actualKeySum)
      cout << "\tValidation Passed" << endl;
    else
      cout << "\tValidation Failed: expected keySum = " << totalKeySum << ", actual keySum = " << actualKeySum << endl;
    std::cout << "\tThroughput = " << totalOps/1000000.0/elapsed_seconds << " Mop/s" << std::endl;
    std::cout << "\tElapsed time = " << elapsed_seconds << " second(s)" << std::endl;

    #ifdef PMEM_STATS
      print_pmem_stats(totalOps);
      // reset_pmem_stats();
    #endif
  }

  void print_name() {
    std::cout << "----------------------------------------------------------------" << std::endl;
    std::cout << "\tDatastructure: " << Set::get_name() << ", " << get_flush_instruction() << endl;
    std::cout << "\tFixed-Size Benchmark: P = " << thread_count << ", size = " << size << 
                 ", Updates = " << update_percent << "%, runtime = " << runtime << "s" << std::endl;
    std::cout << "\tInitialized with " << (processor_count) << " thread(s)" << endl;
    std::cout << "--------------------------------------------------------------" << std::endl;
  }

  Set set;
  const int thread_count, size, range, update_percent, processor_count;
  const double runtime;
};

string concat(vector<string> strings) {
  string str = "";
  for(uint i = 0; i < strings.size(); i++)
    str += strings[i] + ", ";
  return str.substr(0, str.length()-2);
}

template<class Set>
void run_benchmark(const po::variables_map &vm) {
  int threads = vm["threads"].as<int>();
  int size = vm["size"].as<int>();
  int update = vm["update"].as<int>();
  double runtime = vm["runtime"].as<double>();

  FixedSizeBenchmark<Set> benchmark(threads, size, update, runtime);
  benchmark.print_name();
  benchmark.bench();
}

template <template<typename, bool> typename PERSIST>
void process_arguments(po::variables_map& vm) {
  string set = vm["ds"].as<string>();
  string version = vm["version"].as<string>();

  if(set == "list")
  {
    if(version == "original")
      run_benchmark<ListOriginal<int>>(vm);
    else if(version == "auto")
      run_benchmark<ListDurableAutomatic<int, PERSIST>>(vm);
    else if(version == "manual")
      run_benchmark<ListDurableManual<int, PERSIST>>(vm);
    else if(version == "traverse")
      run_benchmark<ListDurableNvTraverse<int, PERSIST>>(vm);    
    else {
      cerr << "Invalid version name" << endl;
      exit(1);        
    }  
  }
  else if(set == "bst")
  {
    if(version == "original")
      run_benchmark<AravindBstOriginal<int>>(vm);
    else if(version == "auto")
      run_benchmark<AravindBstDurableAutomatic<int, PERSIST>>(vm);
    else if(version == "manual")
      run_benchmark<AravindBstDurableManual<int, PERSIST>>(vm);
    else if(version == "traverse")
      run_benchmark<AravindBstDurableNvTraverse<int, PERSIST>>(vm);    
    else {
      cerr << "Invalid version name" << endl;
      exit(1);        
    }  
  }
  else if(set == "hash")
  {
    if(version == "original")
      run_benchmark<HashtableOriginal<int>>(vm);
    else if(version == "auto")
      run_benchmark<HashtableDurableAutomatic<int, PERSIST>>(vm);
    else if(version == "manual")
      run_benchmark<HashtableDurableManual<int, PERSIST>>(vm);
    else if(version == "traverse")
      run_benchmark<HashtableDurableNvTraverse<int, PERSIST>>(vm);    
    else {
      cerr << "Invalid version name" << endl;
      exit(1);        
    }  
  }
  else if(set == "skiplist")
  {
    if(version == "original")
      run_benchmark<SkiplistOriginal<int>>(vm);
    else if(version == "auto")
      run_benchmark<SkiplistDurableAutomatic<int, PERSIST>>(vm);
    else if(version == "manual") {
      run_benchmark<SkiplistDurableManual<int, PERSIST>>(vm);
    } else if(version == "traverse")
      run_benchmark<SkiplistDurableNvTraverse<int, PERSIST>>(vm);
    else {
      cerr << "Invalid version name" << endl;
      exit(1);        
    }    
  }
  else {
    cerr << "Invalid datastructure name" << endl;
    exit(1);    
  }
}

int main(int argc, char *argv[]) {
  // vector<string> persist_names = {"counter", "hash10", "hash15", "hash20", "simple", "link", 
                                  // "interface", "offset"};
  // vector<string> version_names = {"original", "auto", "manual", "traverse"};
  // vector<string> set_names = {"list", "bst", "hash", "skiplist"};

  po::options_description description("Usage:");

  description.add_options()
  ("help,h", "Display this help message")
  ("threads,t", po::value<int>()->default_value(4), "Number of Threads")
  ("size,s", po::value<int>()->default_value(1000), "Initial size of data structure")
  ("update,u", po::value<int>()->default_value(20), "Percentage of Updates (add or remove)")
  ("runtime,r", po::value<double>()->default_value(0.5), "Runtime of Benchmark (milliseconds)")
  ("ds,d", po::value<string>()->default_value("list"), 
                      "Choose one of: list, bst, hash, skiplist")
  ("version,v", po::value<string>()->default_value("auto"), 
                      "Choose one of: original, auto, manual, traverse")
  ("persist,p", po::value<string>()->default_value("counter"), 
                      "Choose one of: counter, hash12/16/20, simple, link, interface");


  po::variables_map vm;
  po::store(po::command_line_parser(argc, argv).options(description).run(), vm);
  po::notify(vm);

  if (vm.count("help")){
    cout << description;
    exit(0);
  }
  
  string persist_type = vm["persist"].as<string>();
  if(persist_type == "counter") process_arguments<persist_counter>(vm);
  // else if(persist_type == "hash") process_arguments<persist_hash>(vm);
  // else if(persist_type == "hash10") process_arguments<persist_hash_cacheline_10>(vm);
  // else if(persist_type == "hash11") process_arguments<persist_hash_cacheline_11>(vm);
  else if(persist_type == "hash12") process_arguments<persist_hash_cacheline_12>(vm);
  // else if(persist_type == "hash13") process_arguments<persist_hash_cacheline_13>(vm);
  // else if(persist_type == "hash14") process_arguments<persist_hash_cacheline_14>(vm);
  // else if(persist_type == "hash15") process_arguments<persist_hash_cacheline_15>(vm);
  else if(persist_type == "hash16") process_arguments<persist_hash_cacheline_16>(vm);
  // else if(persist_type == "hash17") process_arguments<persist_hash_cacheline_17>(vm);
  // else if(persist_type == "hash18") process_arguments<persist_hash_cacheline_18>(vm);
  // else if(persist_type == "hash19") process_arguments<persist_hash_cacheline_19>(vm);
  else if(persist_type == "hash20") process_arguments<persist_hash_cacheline_20>(vm);
  else if(persist_type == "hash23") process_arguments<persist_hash_cacheline_23>(vm);
  else if(persist_type == "hash26") process_arguments<persist_hash_cacheline_26>(vm);
  else if(persist_type == "simple") process_arguments<persist_simple>(vm);
  else if(persist_type == "link") process_arguments<link_and_persist_2>(vm);
  else if(persist_type == "interface") process_arguments<persist_interface>(vm);
  //else if(persist_type == "offset") process_arguments<persist_offset_spec>(vm);
  else {
    cerr << "Invalid persist name" << endl;
    exit(1);
  }

  return 0;
}
