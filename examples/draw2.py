import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np
import os
import palettable
import sys

from matplotlib import rcParams
rcParams['font.family'] = 'serif'
rcParams['font.serif'] = ['Nimbus Roman No9 L', 'DejaVu Serif']

from toolz.dicttoolz import keymap, valmap
from toolz.itertoolz import groupby
from operator import itemgetter
from csv import DictReader

if (len(sys.argv) < 2):
    print "python draw.py 'input file name'"
    sys.exit(1)

input = sys.argv[1]
f = open(input, 'r')
benchmarks = []
raw = {}
keys = ['bp_set_count', 'bp_get_count', 'bp_total', 'rp_set_count', 'rp_get_count', 'rp_total']
legends = ['Base Pointer Initialization', 'Based Pointer Dereference', 'Based Pointer Total', 'Relative Pointer Initialization', 'Relative Pointer Dereference', 'Relative Pointer Total']
for key in keys:
    raw[key] = []

for line in f:
    line = line.strip();
    elems = line.split(' ')
    if (elems[0] == '#'):
        benchmarks.append(elems[1])
    elif (elems[0] in raw):
        raw[elems[0]].append(float(elems[1]));

raw['bp_total'] = np.array(raw['bp_get_count']) + np.array(raw['bp_set_count'])
raw['rp_total'] = np.array(raw['rp_get_count']) + np.array(raw['rp_set_count'])
#execution_time= np.array([1.15358956,  4.30469096,  1.34886492,  3.53642811,  2.64641254]) * 1000
execution_time= np.array([1.15358956,  4.30469096,  3.53642811,  1.33340363,  2.64641254]) * 1000
raw['get'] = (np.array(raw['bp_get_count']) + np.array(raw['rp_get_count'])) / execution_time
raw['set'] = (np.array(raw['bp_set_count']) + np.array(raw['rp_set_count'])) / execution_time
raw['total'] = raw['get'] + raw['set']
keys = ['get', 'set', 'total']
legends = ['Initialization', 'Dereference', 'Total'] 
fig, ax = plt.subplots(1)
fig.set_size_inches(6.0,3.5)
#filename = os.path.splitext(os.path.basename(__file__))[0]

W = 0.25
colors = palettable.tableau.Gray_5.mpl_colors
#colors = palettable.cmocean.sequential.Gray_20.mpl_colors

print benchmarks
rects = []
index = np.arange(len(benchmarks)) * 1.5
for i, key in enumerate(keys):
    rects.append(ax.bar(index + i * W, raw[key], W, color=colors[i], bottom=0.001, log=True))

ax.set_xticks(index + (len(keys) / 2) * W)
ax.set_xticklabels(benchmarks)
plt.xlabel("Benchmark")
plt.ylabel("Frequency(op/ms)")
plt.legend(rects, legends, loc='lower left', bbox_to_anchor=(0, -0.3), ncol=3).draw_frame(False)
plt.savefig(input[: input.rfind('.')] +'.pdf', bbox_inches='tight', format='pdf')


