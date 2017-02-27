import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np
import os
import palettable

from matplotlib import rcParams
rcParams['font.family'] = 'serif'
rcParams['font.serif'] = ['Nimbus Roman No9 L', 'DejaVu Serif']

from toolz.dicttoolz import keymap, valmap
from toolz.itertoolz import groupby
from operator import itemgetter
from csv import DictReader

input = 'result.dat'

f = open(input, 'r')
legends = f.readline().strip().split(' ');
datas = {}
datas_mean = {}

for legend in legends:
    datas[legend] = {}

benchmarks = []
for line in f:
    line = line.strip();
    elems = line.split(" ")
    if (elems[0] == '#'):
        if (not elems[1] in datas[elems[2]]):
            datas[elems[2]][elems[1]] = []
        if (not elems[1] in benchmarks):
            benchmarks.append(elems[1])
        currentArray = datas[elems[2]][elems[1]]
    else:
        currentArray.append(float(line));

for key, value in datas.items():
    datas_mean[key] = [np.array(value[benchmark]).mean() for benchmark in benchmarks]

     
fig, ax = plt.subplots(1)
fig.set_size_inches(6.0,3.5)
filename = os.path.splitext(os.path.basename(__file__))[0]

ind = np.arange(len(benchmarks))
W = 0.25
colors = palettable.tableau.Gray_5.mpl_colors


rects = []

for i, legend in enumerate(legends):
    rects.append(ax.bar(ind + i * W, datas_mean[legend], W, color=colors[i]))

ax.set_xticks(ind + W / 2)
ax.set_xticklabels(benchmarks)
plt.xlabel("Benchmark")
plt.ylabel("Execution Time (seconds)")
plt.legend(rects, legends, loc='upper right').draw_frame(False)
plt.savefig(filename + '.eps', bbox_inches='tight', format='eps')


