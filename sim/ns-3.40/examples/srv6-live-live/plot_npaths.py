import json
import os
import sys
from itertools import islice

import matplotlib
import matplotlib.patches as mpatches
import matplotlib.pyplot as plt
import numpy as np
from flowmon_parser import parse_xml, FiveTuple, Flow, Simulation
from sortedcontainers import SortedDict
import statistics

class OOMFormatter(matplotlib.ticker.ScalarFormatter):
    def __init__(self, order=0, fformat="%1.1f", offset=True, mathText=False):
        self.oom = order
        self.fformat = fformat
        matplotlib.ticker.ScalarFormatter.__init__(self, useOffset=offset, useMathText=mathText)

    def _set_order_of_magnitude(self):
        self.orderOfMagnitude = self.oom

    def _set_format(self, vmin=None, vmax=None):
        self.format = self.fformat
        if self._useMathText:
            self.format = r'$\mathdefault{%s}$' % self.format


figures_path = "figures"

def plot_throughput_npaths_figure(results):
    def plot_throughput_npaths_line(axes, experiment_type, label, color, errorbar_color, marker):
        to_plot = {'x': [], 'y': [], 'miny': [], 'maxy':[]}
        experiments_path = os.path.join(results, experiment_type)
        for n_paths in sorted(os.listdir(experiments_path), key=lambda x: int(x)):
            path = os.path.join(experiments_path, n_paths)
            res = []
            for seed in sorted(os.listdir(path), key=lambda x: int(x)):
                flows_result_path = os.path.join(path, seed, "flow-monitor", "flows_results.json")
                with open(flows_result_path, "r") as f:
                    flows_result = json.load(f)
                res.append(float(flows_result["flows"]["1"]['tx_bit_rate'])/1000)
            to_plot['x'].append(n_paths)
            to_plot['y'].append(statistics.mean(res))
            to_plot['miny'].append(min(res))
            to_plot['maxy'].append(max(res))
            print(experiment_type, res)
        print(experiment_type, to_plot)
        axes.plot(to_plot['x'], to_plot['y'], label=label, linestyle="dashed", fillstyle='none', color=color,
                 marker=marker)
        
        for idx, x in enumerate(to_plot['x']):
            axes.errorbar(x, to_plot['y'][idx], 
                          yerr=[[to_plot['y'][idx] - to_plot['miny'][idx]], 
                                [to_plot['maxy'][idx] - to_plot['y'][idx]]], 
                                color=errorbar_color, elinewidth=1, capsize=1)


    plt.clf()
    plt.grid(linestyle='--', linewidth=0.5)

    def plot_fct_npaths_line(axes, experiment_type, label, color, errorbar_color, marker):
        to_plot = {'x': [], 'y': [], 'miny': [], 'maxy':[]}
        experiments_path = os.path.join(results, experiment_type)
        for n_paths in sorted(os.listdir(experiments_path), key=lambda x: int(x)):
            path = os.path.join(experiments_path, n_paths)
            res = []
            for seed in sorted(os.listdir(path), key=lambda x: int(x)):
                flows_result_path = os.path.join(path, seed, "flow-monitor", "flows_results.json")
                with open(flows_result_path, "r") as f:
                    flows_result = json.load(f)
                res.append(float(flows_result["flows"]["1"]['fct']))

            to_plot['x'].append(n_paths)
            to_plot['y'].append(statistics.mean(res))
            to_plot['miny'].append(min(res))
            to_plot['maxy'].append(max(res))

        axes.plot(to_plot['x'], to_plot['y'], label=label, linestyle="dashed", fillstyle='none', color=color,
                 marker=marker)
        
        for idx, x in enumerate(to_plot['x']):
            axes.errorbar(x, to_plot['y'][idx], 
                          yerr=[[to_plot['y'][idx] - to_plot['miny'][idx]], 
                                [to_plot['maxy'][idx] - to_plot['y'][idx]]], 
                                color=errorbar_color, elinewidth=1, capsize=1)


    plt.clf()
    plt.grid(linestyle='--', linewidth=0.5)

    ax = plt.gca()
    ax2 = ax.twinx()
    ax2.set_ylabel('FCT [ms]', color="goldenrod")
    ax2.set_ylim([0, 5])
    ax2.set_yticks([0, 1, 2, 3, 4, 5])

    plot_throughput_npaths_line(ax, "live-live", "Throughput", "red", "darkred", "o")
    plot_fct_npaths_line(ax2, "live-live", "Flow Completion Time", "goldenrod", "darkgoldenrod", "^")
    # plot_throughput_npaths_line("no-deduplicate", "No Despreader", "blue", "^")

    ax.set_xlabel('N. Paths')
    ax.set_ylabel('Throughput [Mbps]', color="red")
    ax.set_ylim([0, 100])
    # plt.legend(loc='upper center', bbox_to_anchor=(0.5, 1.2), labelspacing=0.2, ncols=3, prop={'size': 6})
    experiment_name = "-".join(results.split("/")[-7:])
    plt.savefig(
        os.path.join(figures_path, f"throughput_npaths_figure_{experiment_name}.pdf"), format="pdf", bbox_inches='tight'
    )

if __name__ == '__main__':
    if len(sys.argv) != 2:
        print(
            "Usage: plot.py <results_path>"
        )
        exit(1)

    results_path = os.path.abspath(sys.argv[1])

    print(f"Results Path: {results_path}")
    print(f"Figures Path: {figures_path}")

    os.makedirs(figures_path, exist_ok=True)

    plt.figure(figsize=(3.5, 2))

    plot_throughput_npaths_figure(results_path)
