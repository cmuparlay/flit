
CXX= g++
FLAGS = -DPWB_IS_CLFLUSH -DALIGNMENT=64


RELEASE_FLAGS= -Wall -O3 -std=c++1z -DPMEM_STATS
DEBUG_FLAGS= -g -fno-omit-frame-pointer -Wall -std=c++1z
# DEBUG_FLAGS+= -fsanitize=address


INCLUDE= -I./ -I./include -I./datastructures
LIB= -lpthread -lboost_program_options

test-hashtable:
	$(CXX) $(DEBUG_FLAGS) $(FLAGS) tests/test-hashtable.cpp -o build/test-hashtable $(INCLUDE) $(LIB)

test-skiplist:
	$(CXX) $(DEBUG_FLAGS) $(FLAGS) tests/test-skiplist.cpp -o build/test-skiplist $(INCLUDE) $(LIB)

test-aravind-bst:
	$(CXX) $(DEBUG_FLAGS) $(FLAGS) tests/test-aravind-bst.cpp -o build/test-aravind-bst $(INCLUDE) $(LIB)

test-harris-linkedlist:
	$(CXX) $(DEBUG_FLAGS) $(FLAGS) tests/test-harris-linkedlist.cpp -o build/test-harris-linkedlist $(INCLUDE) $(LIB)

test: test-aravind-bst test-harris-linkedlist test-skiplist test-hashtable
	./build/test-aravind-bst
	./build/test-harris-linkedlist
	./build/test-skiplist
	./build/test-hashtable

bench:
	$(CXX) $(RELEASE_FLAGS) $(FLAGS) benchmarks/bench_fixed_size.cpp -o build/bench $(INCLUDE) $(LIB)

bench-test:
	$(CXX) $(DEBUG_FLAGS) $(FLAGS) benchmarks/bench_fixed_size.cpp -o build/bench-test $(INCLUDE) $(LIB)

clean:
	rm -r build/*
