import argparse
import os
import sys
import multiprocessing

from create_graphs_paper import *

parser = argparse.ArgumentParser()
parser.add_argument("datastructure", choices=['list', 'bst', 'hash', 'skiplist'], help="datastructure to run")
parser.add_argument("threads", help="Number of threads")
parser.add_argument("size", help="Initial size")
parser.add_argument("versions", choices=['auto', 'manual', 'traverse', '[auto,traverse,manual]'], help="type of persistence transformation")
parser.add_argument("ratios", help="Update ratio, number between 0 and 100.")
parser.add_argument("-f", "--flithash", help="Compare flit hashtable sizes",
                    action="store_true")
parser.add_argument("-n", "--nvram", help="Allocate memory from nvram",
                    action="store_true")
parser.add_argument("-d", "--VMMALLOC_POOL_DIR", type=str)
parser.add_argument("-s", "--VMMALLOC_POOL_SIZE", type=str)
parser.add_argument("-t", "--test_only", help="test script",
                    action="store_true")
parser.add_argument("-g", "--graphs_only", help="graphs only",
                    action="store_true")

# full_ds_name = {
#   'list' : 'List',
#   'bst' : 'Bst',
#   'hash' : 'Hashtable',
#   'skiplist' : 'Skiplist',
# }

args = parser.parse_args()
print("datastructure: " + args.datastructure)
print("threads: " + args.threads)
print("sizes: " + args.size)
print("versions: " + args.versions)
print("ratios: " + args.ratios)
if args.flithash:
  print("Comparing flit hashtable sizes")
if args.nvram:
  print("allocating memory from NVRAM, VMMALLOC_POOL_DIR=" + args.VMMALLOC_POOL_DIR + ", VMMALLOC_POOL_SIZE=" + args.VMMALLOC_POOL_SIZE)
else:
  print("allocating memory from DRAM")

test_only = args.test_only
graphs_only = args.graphs_only

# measured in seconds
runtime = 5 
repeats = 5

if test_only:
  runtime = 0.1
  repeats = 1

#binary location
binary = 'build/bench'

jemalloc='LD_PRELOAD=`jemalloc-config --libdir`/libjemalloc.so.`jemalloc-config --revision`'
nvmmalloc='LD_PRELOAD=libvmmalloc.so.1'
numactl = "numactl --cpubind=0 --membind=0"

persists = ['simple', 'counter', 'hash20', 'link']
if args.datastructure == 'bst':
  persists.remove('link')
if args.datastructure == 'skiplist' and int(args.size) > 100000:
  persists.remove('simple')
if args.flithash:
  persists = ['hash12', 'hash16', 'hash20', 'hash23', 'hash26']


def string_to_list(s):
  s = s.strip().strip('[').strip(']').split(',')
  return [ss.strip() for ss in s]

def to_list(s):
  if type(s) == list:
    return s
  return [s]
    
def runtest(ds,th,size,ver,up,per,scriptfile):
  cmd = ""
  if args.nvram:
    cmd += "echo \'running on NVM\'\n"
    cmd += nvmmalloc + " " + numactl + " "
  else:
    cmd += "echo \'running on DRAM\'\n"
    cmd += jemalloc + " "
  cmd += binary + " " 
  cmd += "--ds " + ds + " --version " + ver + " --persist " + per + " "
  cmd += "--update " + str(up) + " --size " + str(size) + " --threads " + str(th) + " "
  cmd += "--runtime " + str(runtime)
  for i in range(repeats):
    scriptfile.write(cmd + '\n')


datastructure = args.datastructure
exp_type = ""
threads = args.threads
sizes = args.size
versions = args.versions
ratios = args.ratios
if '[' in args.threads:
  exp_type = "scalability"
  threads = string_to_list(threads)
elif '[' in args.versions:
  exp_type = "version"
  versions = string_to_list(versions)
elif '[' in args.ratios:
  exp_type = "ratio"
  ratios = string_to_list(ratios)
else:
  print('invalid argument')
  exit(1)

outfile = "results/" + "-".join([args.datastructure, args.threads, args.size, args.versions, args.ratios, str(args.flithash), str(args.nvram)]) + ".txt"


if not graphs_only:
  scriptfile = open("exp.sh", "w")
  if args.nvram:
    scriptfile.write('export VMMALLOC_POOL_DIR=' + args.VMMALLOC_POOL_DIR + '\n')
    scriptfile.write('export VMMALLOC_POOL_SIZE=' + args.VMMALLOC_POOL_SIZE + '\n')
  for th in to_list(threads):
    for size in to_list(sizes):
      for up in to_list(ratios):
        if not args.flithash:
          runtest(datastructure,th,size,'original',up,'counter',scriptfile)
        for ver in to_list(versions):
          for per in persists:
            runtest(datastructure,th,size,ver,up,per,scriptfile)
  scriptfile.close()
  os.system("bash exp.sh > " + outfile)


throughput = {}
stddev = {}
flushes = {}
stddev_flushes = {}

dss = []
updates = []
versions = []
persists = []
sizes = {}
threads = []

readFile(outfile, throughput, stddev, flushes, stddev_flushes, dss, updates, versions, persists, sizes, threads)

threads.sort()
updates.sort()
for key, value in sizes.items():
  sizes[key].sort()

print(dss)
print(versions)
print(persists)
print(updates)
print(threads)
print(sizes)
print(exp_type)
# print(throughput)

ds = dss[0]
size = sizes[ds][0]
up = updates[0]
th = threads[0]
ver = versions[0]
if ver == 'Original':
  ver = versions[1]

if exp_type == "scalability":
  create_graph_thread(outfile, throughput, stddev, ds, size, up, threads, ver, persists, 'throughput')
elif exp_type == "version":
  create_graph_ver_per(outfile, throughput, stddev, ds, size, up, th, persists, 'throughput')
  create_graph_ver_per(outfile, flushes, stddev_flushes, ds, size, up, th, persists, 'flushes')
elif exp_type == "ratio":
  create_graph_updates(outfile, throughput, stddev, ds, size, updates, th, ver, persists, 'throughput', False, args.flithash)
