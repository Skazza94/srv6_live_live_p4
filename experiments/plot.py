import json
import os
import statistics
import sys

import matplotlib
import matplotlib.pyplot as plt

figures_path = "figures"


def parse_iperf_experiment(path):
    with open(path, "r") as experiment:
        experiment = json.loads(experiment.read())

    if 'end' not in experiment:
        return None

    end_values = experiment['end']['streams'].pop()

    return {
        "bits_per_second": end_values['sender']['bits_per_second'],
        "retr": end_values['sender']['retransmits'],
        "cwd": end_values['sender']['max_snd_cwnd'],
        "avg_rtt": end_values['sender']['mean_rtt']
    }


def parse_n_path_iperf_experiments(directory):
    results = {}

    for test_type in filter(lambda i: not i.startswith("."), os.listdir(directory)):
        results[test_type] = {}
        type_path = os.path.join(directory, test_type)
        for n_path in sorted(map(lambda i: int(i), filter(lambda i: not i.startswith("."), os.listdir(type_path)))):
            n_path_path = os.path.join(type_path, str(n_path))
            results[test_type][n_path] = []
            for experiment in sorted(filter(lambda i: not i.startswith("."), os.listdir(n_path_path))):
                experiment_path = os.path.join(n_path_path, experiment)
                results[test_type][n_path].append(parse_iperf_experiment(experiment_path))

    return results


def plot_n_path_bitrate_figure(results):
    def plot_n_path_bitrate_line(res, color, marker, label, errorbar_color):
        to_plot = {'x': [], 'y': [], 'dy': []}
        for n_path, res in sorted(res.items(), key=lambda item: item[0]):
            values = []
            for result in res:
                if result:
                    values.append(result['bits_per_second'] / 1000000)

            to_plot['x'].append(n_path)
            to_plot['y'].append(statistics.mean(values))
            to_plot['dy'].append(statistics.stdev(values))
        print(label, to_plot, max(to_plot['y']))
        plt.plot(
            to_plot['x'], to_plot['y'], label=label, linestyle='dashed', fillstyle='none', color=color,
            marker=marker
        )
        for idx, x in enumerate(to_plot['x']):
            plt.errorbar(x, to_plot['y'][idx], yerr=to_plot['dy'][idx], color=errorbar_color, elinewidth=1, capsize=1)

    plt.clf()
    plot_n_path_bitrate_line(results['no-deduplicate'], 'goldenrod', "s", "No Deduplication", "darkgoldenrod")
    plot_n_path_bitrate_line(results['live-live'], 'red', "o", "LiveLive", "darkred")

    plt.xticks([2, 3, 4, 5, 6])

    plt.xlabel('N. Paths')
    plt.ylabel('Throughput [Mbps]')
    plt.legend(loc='upper center', bbox_to_anchor=(0.5, 1.2), labelspacing=0.2, ncols=2, prop={'size': 8})
    plt.savefig(
        os.path.join(figures_path, f"n_path_bitrate.pdf"), format="pdf", bbox_inches='tight'
    )


def plot_n_path_cwd_figure(results):
    def plot_n_path_cwd_line(res, color, marker, label, errorbar_color):
        to_plot = {'x': [], 'y': [], 'dy': []}
        for n_path, res in sorted(res.items(), key=lambda item: item[0]):
            values = []
            for result in res:
                if result:
                    values.append(result['cwd'] / 1000)
            to_plot['x'].append(n_path)
            to_plot['y'].append(statistics.mean(values))
            to_plot['dy'].append(statistics.stdev(values))

        plt.plot(
            to_plot['x'], to_plot['y'], label=label, linestyle='dashed', fillstyle='none', color=color,
            marker=marker
        )
        for idx, x in enumerate(to_plot['x']):
            plt.errorbar(x, to_plot['y'][idx], yerr=to_plot['dy'][idx], color=errorbar_color, elinewidth=1, capsize=1)

    plt.clf()
    plot_n_path_cwd_line(results['single'], 'green', "v", "Single", "darkgreen")
    plot_n_path_cwd_line(results['random'], 'blue', "^", "Random", "darkblue")
    plot_n_path_cwd_line(results['live-live'], 'red', "o", "LiveLive", "darkred")

    plt.xticks([2, 3, 4, 5, 6])

    plt.xlabel('N. Paths')
    plt.ylabel('Cwnd Size [KBytes]')
    plt.legend(loc='upper center', bbox_to_anchor=(0.5, 1.2), labelspacing=0.2, ncols=3, prop={'size': 8})
    plt.savefig(
        os.path.join(figures_path, f"n_path_cwd.pdf"), format="pdf", bbox_inches='tight'
    )


def plot_n_path_retry_figure(results):
    def plot_n_path_retry_line(res, color, marker, label, errorbar_color):
        to_plot = {'x': [], 'y': [], 'dy': []}
        for n_path, res in sorted(res.items(), key=lambda item: item[0]):
            if n_path > 6:
                continue

            values = []
            for result in res:
                if result:
                    values.append(result['retr'])

            to_plot['x'].append(n_path)
            to_plot['y'].append(statistics.mean(values))
            to_plot['dy'].append(statistics.stdev(values))

        plt.plot(
            to_plot['x'], to_plot['y'], label=label, linestyle='dashed', fillstyle='none', color=color,
            marker=marker
        )
        for idx, x in enumerate(to_plot['x']):
            plt.errorbar(x, to_plot['y'][idx], yerr=to_plot['dy'][idx], color=errorbar_color, elinewidth=1, capsize=1)

    plt.clf()
    plot_n_path_retry_line(results['single'], 'green', "v", "Single", "darkgreen")
    plot_n_path_retry_line(results['random'], 'blue', "^", "Random", "darkblue")
    plot_n_path_retry_line(results['live-live'], 'red', "o", "LiveLive", "darkred")

    plt.xticks([2, 3, 4, 5, 6])

    plt.xlabel('N. Paths')
    plt.ylabel('N. TCP Retransmissions')
    plt.legend(loc='upper center', bbox_to_anchor=(0.5, 1.2), labelspacing=0.2, ncols=3, prop={'size': 8})
    plt.savefig(
        os.path.join(figures_path, f"n_path_retr.pdf"), format="pdf", bbox_inches='tight'
    )


if __name__ == '__main__':
    if len(sys.argv) != 3:
        print(
            "Usage: plot.py <n_path_results> <figures_path>"
        )
        exit(1)

    n_path_results_path = os.path.abspath(sys.argv[1])
    figures_path = os.path.abspath(sys.argv[2])

    os.makedirs(figures_path, exist_ok=True)

    plt.figure(figsize=(4, 2))
    matplotlib.rc('font', size=8)
    matplotlib.rcParams['hatch.linewidth'] = 0.3
    matplotlib.rcParams['pdf.fonttype'] = 42
    matplotlib.rcParams['ps.fonttype'] = 42

    n_path_results = parse_n_path_iperf_experiments(n_path_results_path)

    plot_n_path_bitrate_figure(n_path_results)
    plot_n_path_cwd_figure(n_path_results)
    plot_n_path_retry_figure(n_path_results)
