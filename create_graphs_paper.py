import csv
import matplotlib as mpl
# mpl.use('Agg')
mpl.rcParams['grid.linestyle'] = ":"
mpl.rcParams.update({'font.size': 20})
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle
import numpy as np
import os
import statistics as st

outdir='graphs'

colors = {
  'persist_simple': 'C0',
  'persist_counter': 'C1',
  'persist_hash_12': 'C2',
  'persist_hash_16': 'C3',
  'persist_hash_20': 'C4',
  'persist_hash_23': 'C0',
  'persist_hash_26': 'C6',
  'link_and_persist': 'C5',
  'Original' : 'C7',
}

names = {
  'persist_simple': 'Plain',
  'persist_counter': 'FliT adjacent',
  'persist_hash_12': 'FliT hashtable-4KB',
  'persist_hash_16': 'FliT hashtable-64KB',
  'persist_hash_20': 'FliT hashtable-1MB',
  'persist_hash_23': 'FliT hashtable-8MB',
  'persist_hash_26': 'FliT hashtable-64MB',
  'link_and_persist': 'Link-and-Persist',
  'Original' : 'Non-Persistent',
}

# pwbes -> flushes
# race should be defined to be two concurrent operations, one of which is a store.
# find an actual implementation online

def toString(ds, ver, per, size, up, th):
  # if ver == 'Auto' and per == 'persist_simple' and size > 100000:
  #   return 'skipped'
  if ver == 'Original' or ver == 'Original-dram':
    return ds + '_' + ver + '_' + str(size) + 's_' + str(up) + 'up_' + str(th) + 'th'
  else:
    return ds + '_' + ver + '_' + per + '_' + str(size) + 's_' + str(up) + 'up_' + str(th) + 'th'

def extractInt(str):
  return int(str.strip(',Mop/s %\n'))

def extractFloat(str):
  return float(str.strip(',Mop/s %\n'))

def avg(numlist):
  total = 0.0
  length = 0
  for num in numlist:
    length=length+1
    total += float(num)
  if length > 0:
    return 1.0*total/length
  else:
    return -1

def export_legend(legend, filename="legend.png", expand=[-5,-5,5,5]):
    fig  = legend.figure
    fig.canvas.draw()
    bbox  = legend.get_window_extent()
    bbox = bbox.from_extents(*(bbox.extents + np.array(expand)))
    bbox = bbox.transformed(fig.dpi_scale_trans.inverted())
    fig.savefig(filename, dpi="figure", bbox_inches=bbox)

def graphFileName(resultsFileName, yaxis):
  resultsFileName = resultsFileName.split('/')[1]
  parameters = resultsFileName.split('-')
  graphName = '-'.join(parameters[:-2])
  if 'True' in parameters[-1]:
    graphName = graphName + '-nvram'
  else:
    graphName = graphName + '-dram'
  if 'True' in parameters[-2]:
    graphName = graphName + '-flithash'
  return graphName + '-' + yaxis + '.png'

def create_graph_thread(resultsfile, results, stddev, ds, size, up, th, ver, persists, yaxis):
  graphsDir = outdir+'/'
  this_graph_title = ds + ' - ' + str(size) + ' keys, ' + str(up) + '% updates, ' + ver

  this_file_name =  graphFileName(resultsfile, yaxis)
  print(this_graph_title)

  series = {}
  errors = {}

  if yaxis == 'flushes':
    persists.remove('persist_simple')

  for per in persists:
    series[per] = []
    errors[per] = []
    for t in th:
      key = toString(ds, ver, per, size, up, t)
      if key in results:
        series[per].append(results[key])
        errors[per].append(stddev[key])
    if len(series[per]) == 0:
      del series[per]
      del errors[per]
  # print('series')
  # print(series)
  original_vers = ['Original']
  for ver in original_vers:
    series[ver] = []
    errors[ver] = []
    for t in th:
      key = toString(ds, ver, '', size, up, t)
      series[ver].append(results[key])
      errors[ver].append(stddev[key])
  fig, axs = plt.subplots()
  rects = {}
  # print(series)
  for key in series:
    # print(str(len(series[key])) + " " + str(len(th)))
    if len(series[key]) == 0:
      continue
    rects[key] = axs.errorbar(th, 
                         series[key], 
                         errors[key], capsize=3, 
                         marker='o',
                         color=colors[key],
                         # hatch=hatches[alg],
                         label=names[key],
                        )

  # cur_ylim = axs.get_ylim()

  axs.set_ylim(bottom=0)
  if yaxis == 'throughput': 
    axs.set(ylabel='Throughput (Mop/s)')
  elif yaxis == 'flushes':
    axs.set(ylabel='Flushes per Operation')
  axs.set(xlabel='Number of Threads')
  plt.grid()
  legend_x = 1
  legend_y = 0.9
  plt.legend(bbox_to_anchor=(legend_x, legend_y))
  plt.title(this_graph_title)
  plt.savefig(graphsDir+this_file_name, bbox_inches='tight')
  plt.close('all')

def create_graph_updates(resultsfile, results, stddev, ds, size, updates, th, ver, persists, yaxis, printLegend=False, htsize= False, legendName=''):
  width = 0.3  # the width of the bars

  graphsDir = outdir+'/'
  # print(ver)
  this_graph_title = yaxis + ': ' + ds + ' - ' + str(size) + ' keys, ' + ver + ' - ' + str(th) + ' threads'
  if htsize:
    this_graph_title = 'htsize-' + this_graph_title
  this_file_name = graphFileName(resultsfile, yaxis)
  print(this_graph_title)

  series = {}
  errors = {}
  # if htsize:
  #   persists = ['persist_hash_12', 'persist_hash_16', 'persist_hash_20', 'persist_hash_23', 'persist_hash_26']
  # else:
  #   persists = ['persist_simple', 'persist_counter', 'persist_hash_20', 'link_and_persist']
  if yaxis == 'flushes':
    persists.remove('persist_simple')

  for per in persists:
    series[per] = []
    errors[per] = []
    for up in updates:
      key_original = toString(ds, 'Original', per, size, up, th)
      if per == 'Original':
        key = toString(ds, 'Original', per, size, up, th)
      else:
        key = toString(ds, ver, per, size, up, th)
      if key in results:
        # print(key_original + ' ' + str(results[key_original]))
        if yaxis == 'throughput' and not htsize:
          series[per].append(1.0*results[key]/results[key_original])
        else:
          series[per].append(results[key])
        errors[per].append(stddev[key])
    if len(series[per]) == 0:
      del series[per]
      del errors[per]

  fig, axs = plt.subplots()
  rects = {}
  x = np.arange(len(updates))*(len(series)+3)*(width)
  curpos = x - (len(series)-1)/2.0*width
  i = 0
  for per in series:
    if len(series[per]) == 0:
      continue
    rects[per] = axs.bar(curpos, 
                         series[per], 
                         width, 
                         label=names[per],
                         color=colors[per],
                         # hatch=hatches[alg],
                         # yerr=errors[per], error_kw={'capsize':4}
                        )
    curpos += width
    # i+=1
    # if i == 2:
    #   curpos += 0.3

  cur_ylim = axs.get_ylim()

  axs.set_xticks(x)
  axs.set_xticklabels(updates)
  
  if yaxis == 'throughput' and htsize:
    axs.set(ylabel='Throughput (Mop/s)')
  elif yaxis == 'throughput': 
    axs.set(ylabel='Normalized Throughput')
    axs.set_ylim(bottom=0,top=1.03)
  elif yaxis == 'flushes':
    axs.set(ylabel='Flushes per Operation')
  axs.set(xlabel='Update Percentage')

  if printLegend:
    legend_x = 1
    legend_y = 0.5 
    legend = plt.legend(loc='center left', bbox_to_anchor=(legend_x, legend_y), ncol=7, framealpha=0.0)
    # if htsize:
    #   export_legend(legend, graphsDir+yaxis+'_compare_updates_ht_size_legend.png')
    # else:
    export_legend(legend, graphsDir+yaxis+'_compare_updates_legend.png')
  else:
    plt.grid()
    legend_x = 1
    legend_y = 0.9
    if htsize:
      plt.legend(bbox_to_anchor=(legend_x, legend_y))
    else:
      plt.legend(bbox_to_anchor=(legend_x, legend_y))
      plt.axhline(y=1,linewidth=2, color='k', linestyle='--')
    plt.title(this_graph_title)
    plt.savefig(graphsDir+this_file_name, bbox_inches='tight')
    plt.close('all')

def create_graph_ver_per(resultsfile, results, stddev, ds, size, up, th, persists, yaxis, printLegend=False):
  xlabels = ['Automatic', 'NvTraverse', 'Manual']
  width = 0.3  # the width of the bars

  graphsDir = outdir+'/'
  this_graph_title = yaxis + ': ' + ds + ' - ' + str(size) + ' keys, ' + str(up) + '% updates, ' + str(th) + ' threads'
  this_file_name = graphFileName(resultsfile, yaxis)
  print(this_graph_title)

  series = {}
  errors = {}
  versions = ['Auto', 'NvTraverse', 'Manual']
  if yaxis == 'flushes':
    persists.remove('persist_simple')

  for per in persists:
    series[per] = []
    errors[per] = []
    for ver in versions:
      key = toString(ds, ver, per, size, up, th)
      if key in results:
        series[per].append(results[key])
        errors[per].append(stddev[key])
    if len(series[per]) == 0:
      del series[per]
      del errors[per]

  fig, axs = plt.subplots()
  rects = {}
  x = np.arange(len(versions))*(len(series)+2)*(width)
  curpos = x - (len(series)-1)/2.0*width
  i = 0
  for per in series:
    if len(series[per]) == 0:
      continue
    rects[per] = axs.bar(curpos, 
                         series[per], 
                         width, 
                         label=names[per],
                         color=colors[per],
                         # hatch=hatches[alg],
                         # yerr=errors[per], error_kw={'capsize':4}
                        )
    curpos += width
    # i+=1
    # if i == 2:
    #   curpos += 0.3

  cur_ylim = axs.get_ylim()
  throughput_original = results[toString(ds, 'Original', '', size, up, th)]
  # throughput_original_dram = results[toString(ds, 'Original-dram', '', size, up, th)]
  throughput_original_dram = 0

  axs.set_xticks(x)
  axs.set_xticklabels(xlabels)
  if yaxis == 'throughput':
    axs.set_ylim(bottom=0,top=max(max(throughput_original_dram, throughput_original), cur_ylim[1])*1.03)
  if yaxis == 'throughput': 
    axs.set(ylabel='Throughput (Mop/s)')
  elif yaxis == 'flushes':
    axs.set(ylabel='Flushes per Operation')

  if printLegend:
    legend_x = 1
    legend_y = 0.5 
    legend = plt.legend(loc='center left', bbox_to_anchor=(legend_x, legend_y), ncol=7, framealpha=0.0)
    export_legend(legend, graphsDir+yaxis+'_compare_versions_legend.png')
  else:
    plt.grid()
    legend_x = 1
    legend_y = 0.9
    # if includeLegend:
    #   plt.legend()
    plt.legend(bbox_to_anchor=(legend_x, legend_y))
    plt.title(this_graph_title)
    if yaxis == 'throughput': 
      plt.axhline(y=throughput_original,linewidth=2, color='k', linestyle='--')
    # plt.axhline(y=throughput_original_dram,linewidth=1, color='b', linestyle='--')
    plt.savefig(graphsDir+this_file_name, bbox_inches='tight')
    plt.close('all')

def readFile(filename, throughput, stddev, flushes, stddev_flushes, dss, updates, versions, persists, sizes, threads):
  throughputRaw = {}
  flushesRaw = {}
  th = 0
  size = 0
  up = 0
  ds = ""
  version = ""
  persist = ""

  hash_ver = ['persist_hash_12', 'persist_hash_16', 'persist_hash_20'];
  hash_counter = 0;

  file = open(filename, 'r')
  for line in file.readlines():
    if line.find('Datastructure:') != -1:
      ll = line.split(' ')
      ds = ll[1]
      version = ll[2].strip(',')
      persist = ll[3].strip(',')
    if line.find('Benchmark') != -1:
      ll = line.split(' ')
      th = extractInt(ll[4])
      size = extractInt(ll[7])
      up = extractInt(ll[10])
    if line.find('Throughput') != -1:
      if persist == 'persist_hash_cacheline':
        # (repeats * threads * sizes * updates) % number of different hash versions
        persist = hash_ver[int(hash_counter/(5*3*2*3))%3];
        hash_counter += 1
        # print("This shouldn't happen")
      key = toString(ds, version, persist, size, up, th)
      if ds not in dss:
        dss.append(ds)
      if version not in versions:
        versions.append(version)
      if version != 'Original' and persist not in persists:
        persists.append(persist)
      if ds not in sizes:
        sizes[ds] = []
      if size not in sizes[ds]:
        sizes[ds].append(size)
      if up not in updates:
        updates.append(up)
      if th not in threads:
        threads.append(th)
      if key not in throughputRaw:
        throughputRaw[key] = []
      # print(line.split(' '))
      throughputRaw[key].append(extractFloat(line.split(' ')[2]))
    if line.find('Flushes per operation') != -1:
      key = toString(ds, version, persist, size, up, th)
      if key not in flushesRaw:
        flushesRaw[key] = []
      flushesRaw[key].append(float(line.split(' ')[3]))

  # print(throughputRaw)
  # Average throughputRaw into throughput

  print("incorrect number of trials:")
  # for key in throughputRaw:
  #   resultsAll = throughputRaw[key]
  #   if len(resultsAll) != 5:
      # print(key + ': ' + str(len(resultsAll)))
      # print('\t' + str(resultsAll))

  for key in throughputRaw:
    # print(key)
    results = throughputRaw[key]
    flush_results = flushesRaw[key]
    # print(results)
    throughput[key] = avg(results)
    flushes[key] = avg(flush_results)
    stddev[key] = st.pstdev(results)
    stddev_flushes[key] = st.pstdev(flush_results)
    # print(avgResult)
  throughput['skipped'] = 0
  stddev['skipped'] = 0
  flushes['skipped'] = 0
  stddev_flushes['skipped'] = 0
