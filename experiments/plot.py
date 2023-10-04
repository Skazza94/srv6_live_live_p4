import os
import statistics

import matplotlib
import matplotlib.pyplot as plt
import matplotlib.ticker as ticker
import numpy as np
import os
import statistics
import sys
from distutils.util import strtobool

FIGURES_PATH = "figures"

def parse_iperf_experiment(path):
    result = {}
    with open(path, "r") as experiment:
        experiment = experiment.readlines()[-4:-2]
    interval, _, transfer, _, bitrate, _, retr, role = experiment[0].strip().split()[2:]
    result[role] = {"transfer": float(transfer), "bitrate": float(bitrate), "retr": int(retr)}

    interval, _, transfer, _, bitrate, _, role = experiment[1].strip().split()[2:]
    result[role] = {"transfer": float(transfer), "bitrate": float(bitrate), "retr": int(retr)}

    return result


def parse_iperf_experiments(directory):
    results = {}
    for delay in sorted(map(lambda i: int(i), filter(lambda i: not i.startswith("."), os.listdir(directory)))):
        delay_folder = os.path.join(directory, str(delay))
        results[delay] = []
        for experiment in sorted(filter(lambda i: not i.startswith("."), os.listdir(delay_folder))):
            experiment_path = os.path.join(delay_folder, experiment)
            results[delay].append(parse_iperf_experiment(experiment_path))
    return results


def plot_latency_bitrate_figure(live_live_results):
    def plot_latency_bitrate_figure_line(role, color, marker, label, errorbar_color):
        to_plot = {'x': [], 'y': [], 'dy': []}
        for delay, results in sorted(live_live_results.items(), key=lambda item: item[0]):
            values = []
            for result in results:
                if result:
                    values.append(result[role]['bitrate'])

            to_plot['x'].append(delay)
            to_plot['y'].append(statistics.mean(values))
            to_plot['dy'].append(statistics.stdev(values))

        plt.plot(to_plot['x'], to_plot['y'], label=label, linestyle='dashed', fillstyle='none', color=color, marker=marker)

        for idx, x in enumerate(to_plot['x']):
            plt.errorbar(x, to_plot['y'][idx], yerr=to_plot['dy'][idx], color=errorbar_color, elinewidth=1, capsize=1)

    plt.clf()
    ax = plt.gca()
    plot_latency_bitrate_figure_line("receiver", 'red', "o", "Receiver", "darkred")
    # plot_insertions_table_size_line(os.path.join(results_path, "avg"), 'royalblue', "^", "2-Pkt Flows", "blue")
    # plot_insertions_table_size_line(os.path.join(results_path, "best"), 'green', "s", "8-Pkt Flows", "darkgreen")

    plt.xlabel('Latency [ms]')
    plt.ylabel('Bitrate [Mbps]')
    plt.legend(loc=(0.65, 0.55), labelspacing=0.2, prop={'size': 8})
    plt.savefig(os.path.join(FIGURES_PATH, "insertions_table_size.pdf"), format="pdf", bbox_inches='tight')


if __name__ == '__main__':
    os.makedirs(FIGURES_PATH, exist_ok=True)

    plt.figure(figsize=(4, 2))
    matplotlib.rc('font', size=8)
    matplotlib.rcParams['hatch.linewidth'] = 0.3
    matplotlib.rcParams['pdf.fonttype'] = 42
    matplotlib.rcParams['ps.fonttype'] = 42

    live_live_results = parse_iperf_experiments("/home/tommaso/Code/srv6_live_live_p4/experiments/results/live-live/")
    plot_latency_bitrate_figure(live_live_results)
