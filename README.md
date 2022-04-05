# FliT: A Library for Simple and Efficient Persistent Algorithms

This repository contains the artifact for the following research project. If you use it for scientific purposes, please cite it as follows:

> **FliT: A Library for Simple and Efficient Persistent Algorithms**<br />
> Yuanhao Wei, Naama Ben-David, Michal Friedman, Guy E. Blelloch, Erez Petrank<br />
> ACM Symposium on Principles and Practice of Parallel Programming (PPoPP 2022), 2022

## Hardware Requirements
  - A multicore machine (preferably with 48+ cores)
  - A newer Intel CPU that supports CLFLUSHOPT or CLWB is preferable, but the artifact will also work with the older CLFLUSH instruction. See below for instructions on how to set the default flush instruction.
  - A machine with Intel Optane DC persistent memory would be ideal, however similar results can be achieved on regular DRAM. We include instructions for running on both persistent memory (NVRAM) and DRAM.

## Software Requirements
  - Operating System: Ubuntu 16.04+
  - compiler: g++-9 or higher
  - Boost C++ Libraries: boost_program_options
    - sudo apt-get install -y libboost-program-options-dev
  - jemalloc-5.2.1: https://github.com/jemalloc/jemalloc/releases/download/5.2.1/jemalloc-5.2.1.tar.bz2
    - To install, extract and follow instructions in the INSTALL text file
    - Then run ```sudo apt install libjemalloc-dev```
  - python3 (we use version 3.8.10) with matplotlib (we use version 3.1.2)
    - sudo apt-get install python3-matplotlib
  - Running on NVRAM:
    - PMDK (for libvmmalloc)
    - numactl (for restricting to a single socket)

## Getting started / Running tests
  - Download and extract the artifact archive from the submission site
  - ```cd``` into the flush-mark-lib directory and run ```make test``` to compile and run some basic tests
  - expected output for ```make test``` can be found in ```make_test_expected_output.txt```

## Configuring and compiling benchmark
  - The default flush instruction is CLFLUSH. To change this, open ```Makefile``` and change ```-DPWB_IS_CLFLUSH``` on line 3 to either ```-DPWB_IS_CLFLUSHOPT``` or ```-DPWB_IS_CLWB```
  - Then compile the benchmark using ```make bench```

## Benchmarking (DRAM)
  - Note: these steps assume ```make bench``` has already been executed
  - To reproduce all the graphs in the paper on DRAM, run ```bash runall-dram.sh```
    - this command will take ~5 hours to run
  - The output graphs will be stored in the graphs/ directory
  - You can rerun a specific graph by running the corresponding command from the runall-dram.sh file. Each command generates a single graph.
  - You can also run custom experiments (and generate graphs for them) using the following script: 

```
    python3 run_experiments.py [datastructure] [threads] [size] [versions] [ratios] [-f] [-n] [-d] [-s]
```
  - See runall-dram.sh and runall-nvram.sh for examples of how to use the above script
  - Parameter description: 
    - datastructure: data structure to run, pick one of ['list', 'bst', 'hash', 'skiplist']
    - threads: number of threads (can be a list)
    - size: initial size
    - versions: type of persistence transformation, pick one of ['auto', 'manual', 'traverse', '[auto,traverse,manual]']
    - ratios: percentage of updates, number between 0 and 100 (can also be a list)
    - Exactly one of the parameters [threads, versions, ratios] has to be a list. This parameter will be used as the x-axis of the generated graph.
    - [-f]: Compare flit hashtable sizes (Used for Figure 4)
    - [-n]: Dynamically allocate memory from nvram using libvmmalloc
    - [-d]: value to bind to environment variable VMMALLOC_POOL_DIR  (used by libvmmalloc)
    - [-s]: value to bind to environment variable VMMALLOC_POOL_SIZE (used by libvmmalloc)

## Benchmarking (NVRAM)
  - To reproduce all the graphs in the paper on NVRAM:
    1) configure the machine to App-Direct mode, 
    2) ensure that libvmmalloc works, 
    3) configure the VMMALLOC_POOL_DIR parameter [-d] in runall-nvram.sh accordingly, 
    4) run ```bash runall-nvram.sh```
  - As before, the output graphs will be stored in the graphs/ directory and you can rerun a specific graph by running the corresponding command from the runall-nvram.sh file.
  - Note that NVRAM experiments will restrict to running on a single socket (using numactl). We observed poor cross socket performance with NVRAM.
  - As before, custom experiments can be run using run_experiments.py (See instructions from previous section).
