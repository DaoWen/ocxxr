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

#yindex = np.arange(6)
fig, ax = plt.subplots(1)
fig.set_size_inches(8.0,3.5)
#plt.semilogy(yindex, np.exp(yindex))
#filename = os.path.splitext(os.path.basename(__file__))[0]

W = 0.2
colors = palettable.tableau.Gray_5.mpl_colors


rects = []
index = np.arange(len(benchmarks)) * 1.5
for i, key in enumerate(keys):
    rects.append(ax.bar(index + i * W, raw[key], W, color=colors[0], log=True))

ax.set_xticks(index + W)
ax.set_xticklabels(benchmarks)
plt.xlabel("Pointer Type")
plt.ylabel("Slowdown")
plt.legend(rects, legends, loc='lower left', bbox_to_anchor=(0, -0.5), ncol=2).draw_frame(False)
plt.savefig(input[: input.rfind('.')] +'.pdf', bbox_inches='tight', format='pdf')

