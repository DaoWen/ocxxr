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

base_line_name="native"
input = sys.argv[1]
f = open(input, 'r')
legends = f.readline().strip().split(' ');
raw = {}
datas = {}
datas['mean'] = {}
datas['error'] = {}

for legend in legends:
    raw[legend] = []

for line in f:
    line = line.strip();
    elems = line.split(" ")
    if (elems[0] == '#'):
        currentArray = raw[elems[2]]
    else:
        currentArray.append(float(line));

for key, value in raw.items():
    datas['mean'][key] = np.array(value).mean();

base_line = datas['mean'][base_line_name]
for key, value in datas['mean'].items():
    datas['mean'][key] /= base_line
for key, value in raw.items():
    standard_error = np.std(np.array(value) / base_line) / np.sqrt(len(value))
    datas['error'][key] = [[standard_error], [standard_error]]
    #datas['error'][key] = standard_error

fig, ax = plt.subplots(1)
fig.set_size_inches(4.0,3.5)
filename = os.path.splitext(os.path.basename(__file__))[0]

W = 0.5
colors = palettable.tableau.Gray_5.mpl_colors
errkw=dict(ecolor='black', lw=2, capthick=1)


rects = []

for i, legend in enumerate(legends):
    rects.append(ax.bar(i + W, datas['mean'][legend], W, color=colors[0], yerr=datas['error'][legend], error_kw=errkw))

ax.set_xticks(np.arange(len(legends)) + W)
ax.set_xticklabels(legends)
plt.xlabel("Pointer Type")
plt.ylabel("Slowdown")
#plt.legend(rects, map(lambda x: x.replace('_', ' ').title() + " Pointers", legends), loc='lower left', bbox_to_anchor=(0, -0.3), ncol=2).draw_frame(False)
plt.savefig(input[: input.rfind('.')] +'.pdf', bbox_inches='tight', format='pdf')


