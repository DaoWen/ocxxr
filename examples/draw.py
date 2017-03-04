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

base_line_index = 0
input = 'result.dat'
f = open(input, 'r')
legends = f.readline().strip().split(' ');
raw = {}
datas = {}
datas['mean'] = {}
datas['error'] = {}
for legend in legends:
    raw[legend] = {}

benchmarks = []
for line in f:
    line = line.strip();
    elems = line.split(" ")
    if (elems[0] == '#'):
        if (not elems[1] in raw[elems[2]]):
            raw[elems[2]][elems[1]] = []
        if (not elems[1] in benchmarks):
            benchmarks.append(elems[1])
        currentArray = raw[elems[2]][elems[1]]
    else:
        currentArray.append(float(line));

for key, value in raw.items():
    datas['mean'][key] = np.array([np.array(value[benchmark]).mean() for benchmark in benchmarks])

base_line = [x for x in datas['mean'][legends[base_line_index]]]
for key, value in datas['mean'].items():
    datas['mean'][key] = np.array(value) / np.array(base_line)

for key, value in raw.items():
    standard_error = np.array([np.std(np.array(value[benchmark]) / base_line[i]) / np.sqrt(len(value[benchmark])) for i, benchmark in enumerate(benchmarks)])
    datas['error'][key] = [standard_error, standard_error]

fig, ax = plt.subplots(1)
fig.set_size_inches(6.0,3.5)
filename = os.path.splitext(os.path.basename(__file__))[0]

ind = np.arange(len(benchmarks))
W = 0.25
colors = palettable.tableau.Gray_5.mpl_colors
errkw=dict(ecolor='black', lw=2, capthick=1)


rects = []

for i, legend in enumerate(legends):
    rects.append(ax.bar(ind + i * W, datas['mean'][legend], W, color=colors[i], yerr=datas['error'][legend], error_kw=errkw))

ax.set_xticks(ind + W / 2)
ax.set_xticklabels(benchmarks)
plt.xlabel("Benchmark")
plt.ylabel("Slowdown")
plt.legend(rects, map(lambda x: x.replace('_', ' ').title() + " Pointers", legends), loc='lower left', bbox_to_anchor=(0, -0.3), ncol=2).draw_frame(False)
plt.savefig(filename + '.eps', bbox_inches='tight', format='eps')


