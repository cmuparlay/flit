
# Figure 4
python3 run_experiments.py bst 44 10000 auto [0,5,50] -f -n -d /mnt/pmem0/flush_mark_lib -s 80000000000

# Figure 5
python3 run_experiments.py bst [1,6,12,18,24,30,36,42,48] 10000 auto 5 -n -d /mnt/pmem0/flush_mark_lib -s 80000000000

# Figure 6a
python3 run_experiments.py bst 44 10000 [auto,traverse,manual] 5 -n -d /mnt/pmem0/flush_mark_lib -s 80000000000

# Figures 6b and 8a
python3 run_experiments.py hash 44 10000 [auto,traverse,manual] 5 -n -d /mnt/pmem0/flush_mark_lib -s 80000000000

# Figures 6c and 8b
python3 run_experiments.py list 44 128 [auto,traverse,manual] 5 -n -d /mnt/pmem0/flush_mark_lib -s 80000000000

# Figure 6d
python3 run_experiments.py skiplist 44 10000 [auto,traverse,manual] 5 -n -d /mnt/pmem0/flush_mark_lib -s 80000000000

# Figure 7a
python3 run_experiments.py bst 44 10000 auto [0,5,50] -n -d /mnt/pmem0/flush_mark_lib -s 80000000000

# Figure 7b
python3 run_experiments.py hash 44 10000 auto [0,5,50] -n -d /mnt/pmem0/flush_mark_lib -s 80000000000

# Figure 7c
python3 run_experiments.py list 44 128 auto [0,5,50] -n -d /mnt/pmem0/flush_mark_lib -s 80000000000

# Figure 7d
python3 run_experiments.py skiplist 44 10000 auto [0,5,50] -n -d /mnt/pmem0/flush_mark_lib -s 80000000000

# Figure 7e
python3 run_experiments.py bst 44 10000000 auto [0,5,50] -n -d /mnt/pmem0/flush_mark_lib -s 80000000000

# Figure 7f
python3 run_experiments.py hash 44 10000000 auto [0,5,50] -n -d /mnt/pmem0/flush_mark_lib -s 80000000000

# Figure 7g
python3 run_experiments.py list 44 4000 auto [0,5,50] -n -d /mnt/pmem0/flush_mark_lib -s 80000000000

# Figure 7h, warning: takes several hours to run
python3 run_experiments.py skiplist 44 10000000 auto [0,5,50] -n -d /mnt/pmem0/flush_mark_lib -s 80000000000