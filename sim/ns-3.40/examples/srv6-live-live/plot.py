import json
import os
import statistics
import sys
from datetime import datetime

import matplotlib
import matplotlib.pyplot as plt

figures_path = "figures"


def plot_seqn_figure(results, experiment):
    def plot_seqn_line(path, ll_port, color, marker, label, errorbar_color):
        to_plot = {'x': [], 'y': [], 'dy': []}
        with open(os.path.join(path, "log.txt"), "r") as f:
            seqn_lines = f.readlines()

        starting_ts = None
        for line in seqn_lines:
            line = line.strip().split()
            port = int(line[2])
            if port == ll_port:
                ts = datetime.fromtimestamp(int(line[0])/10**9).timestamp()
                if not starting_ts:
                    starting_ts = ts

                seqn = int(line[1])
                print(ts - starting_ts, seqn)
                to_plot['x'].append(ts - starting_ts)
                to_plot['y'].append(seqn)

        plt.plot(to_plot['x'], to_plot['y'], label=label, linestyle="dashed", fillstyle='none', color=color,
                 marker=marker)

    plt.clf()
    plot_seqn_line(os.path.join(results, experiment), 2,
                   'blue', None, "Backup", "darkblue")
    plot_seqn_line(os.path.join(results, experiment), 1,
                   'red', None, "Active", "darkred")
    # plt.xticks([0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10])

    plt.xlabel('Time (s)')
    plt.ylabel('Sequence Number')
    plt.legend(loc='upper center', bbox_to_anchor=(0.5, 1.2), labelspacing=0.2, ncols=3, prop={'size': 8})
    plt.savefig(
        os.path.join(figures_path, f"seqn_figure_{experiment}.pdf"), format="pdf", bbox_inches='tight'
    )


if __name__ == '__main__':
    if len(sys.argv) != 3:
        print(
            "Usage: plot.py <results_path> <figures_path>"
        )
        exit(1)

    results_path = os.path.abspath(sys.argv[1])
    figures_path = os.path.abspath(sys.argv[2])

    print(f"Results Path: {results_path}")
    print(f"Figures Path: {figures_path}")

    os.makedirs(figures_path, exist_ok=True)

    plt.figure(figsize=(4, 2))

    plot_seqn_figure(results_path, "1-1-10")
