import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np
import os
import brewer2mpl

from matplotlib import rcParams
rcParams['font.family'] = 'serif'
rcParams['font.serif'] = ['Nimbus Roman No9 L', 'DejaVu Serif']

from toolz.dicttoolz import keymap, valmap
from toolz.itertoolz import groupby
from operator import itemgetter
from csv import DictReader

input = 'result.dat'

f = open(input, 'r')
offset = {}
native = {}
keys = []
for line in f:
    line = line.strip();
    elems = line.split(" ")
    if (elems[0] == '#'):
        if (elems[2] == 'offset'):
            currentArray = offset[elems[1]] = []
            keys.append(elems[1])
        else:
            currentArray = native[elems[1]] = []
    else:
        currentArray.append(float(line));

offset_mean = [np.array(offset[key]).mean() for key in keys]
native_mean = [np.array(native[key]).mean() for key in keys]


fig, ax = plt.subplots(1)
fig.set_size_inches(6.0,3.5)
filename = os.path.splitext(os.path.basename(__file__))[0]

ind = np.arange(len(keys))
W = 0.25
colors = brewer2mpl.get_map('Set2', 'qualitative', 8).mpl_colors

rects0 = ax.bar(ind, offset_mean, W, color=colors[0])
rects1 = ax.bar(ind + W, native_mean ,W, color=colors[1]);

ax.set_xticks(ind + W / 2)
ax.set_xticklabels(keys)
plt.xlabel("Benchmark")
plt.ylabel("Execution Time (seconds)")
plt.legend((rects0, rects1), ('offset', 'native'), loc='upper right').draw_frame(False)
plt.savefig(filename + '.eps', bbox_inches='tight', format='eps')


