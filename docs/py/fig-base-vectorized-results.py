#=========================================================================
# fig-parallel-node-results.py
#=========================================================================

import matplotlib.pyplot as plt
import math
import sys
import os.path
import numpy as np

#-------------------------------------------------------------------------
# Calculate figure size
#-------------------------------------------------------------------------
# We determine the fig_width_pt by using \showthe\columnwidth in LaTeX
# and copying the result into the script. Change the aspect ratio as
# necessary.

fig_width_pt  = 244.0
inches_per_pt = 1.0/72.27                     # convert pt to inch

aspect_ratio  = 0.70

fig_width     = 6.5                           # width in inches
fig_height    = fig_width * aspect_ratio      # height in inches
fig_size      = [ fig_width, fig_height ]

#-------------------------------------------------------------------------
# Configure matplotlib
#-------------------------------------------------------------------------

plt.rcParams['pdf.use14corefonts'] = True
plt.rcParams['font.size']          = 16
plt.rcParams['font.family']        = 'serif'
plt.rcParams['font.serif']         = ['Times']
plt.rcParams['figure.figsize']     = fig_size

#-------------------------------------------------------------------------
# Raw data
#-------------------------------------------------------------------------

# Benchmarks

bmarks = [
  'base serial',
  'tuned serial',
]

num_bmarks = len( bmarks )

# Configurations

configs = [
  '200x200',
  '400x400',
  '800x800',
  '1600x1600',
]

num_configs = len( configs )

# Results (execution time in seconds)

perf_data = [

  # base         tuned

  # 200

  [
    2.355, 1.224416017532349
  ],

  # 400
  [
    14.962, 9.824005842208862
  ],

  # 800
  [
    120.285, 82.19018602371216
  ],

  # 1600
  [
    929.226, 765.4277429580688
  ]
]

# perf_data = [ np.array( perf_data[0] ) / np.array( data ) for data in perf_data ]

#-------------------------------------------------------------------------
# Plot parameters
#-------------------------------------------------------------------------

# Setup x-axis

ind = np.arange( num_bmarks )
mid = num_configs / 2.0

# Bar widths

width = 0.10

# Colors

colors = [
  '#FFCCCC',
  # '#FF9999',
  # '#FF6666',
  # '#FF3333',
  '#FF0000',
  # '#CC0000',
  # '#990000',
  # '#660000',
  # '#ff6666',
  # '#66cccc',
  # '#ff9966',
  # '#33cc99',
  # '#ff99cc',
  '#66cc99',
  # '#ff99cc',
  # '#66cc99',
  # '#ff99cc',
  # '#ffff99',
  # '#ffff99',
  '#ffff99',
]

#-------------------------------------------------------------------------
# Create plot
#-------------------------------------------------------------------------

# Initialize figure

fig = plt.figure()
ax  = fig.add_subplot(111)

# Plot formatting

ax.set_xticks( ind+mid*width+width )
ax.set_xticklabels( bmarks )

ax.set_xlabel( 'Run Type', fontsize=16 )
ax.set_ylabel( 'Total Execution Time in Seconds',            fontsize=16 )

ax.grid(True)

# Set axis limits

plt.axis( xmax=num_bmarks-1+(num_configs+2)*width, ymax=930.0)#6.5 )

# Add bars for each configuration

rects = []

for i, perf in enumerate( perf_data ):
  rects.append( ax.bar( ind+width*i+width, perf, width, color=colors[i] ) )

# Set tick positions

ax.xaxis.set_ticks_position('bottom')
ax.yaxis.set_ticks_position('left')

# Add horizontal line for baseline

plt.axhline( y=1, color='k', linewidth=1.5 )

# Legend

ax.legend( rects, configs, loc=8, bbox_to_anchor=(0.01,1.02,0.98,0.1),
           ncol=4, borderaxespad=0, prop={'size':12}, frameon=False )

# Pretty layout

plt.tight_layout()

# Turn off top and right border

ax.xaxis.grid(False)
ax.spines['right'].set_visible(False)
ax.spines['top'].set_visible(False)
ax.xaxis.set_ticks_position('bottom')
ax.yaxis.set_ticks_position('left')

#-------------------------------------------------------------------------
# Generate PDF
#-------------------------------------------------------------------------

input_basename = os.path.splitext( os.path.basename(sys.argv[0]) )[0]
output_filename = input_basename + '.py.pdf'
plt.savefig( output_filename, bbox_inches='tight' )

