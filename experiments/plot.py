import os
import statistics

import matplotlib
import matplotlib.pyplot as plt

FIGURES_PATH = "figures"


def parse_iperf_experiment(path):
    result = {}
    with open(path, "r") as experiment:
        experiment = experiment.readlines()[-4:-2]
    interval, _, transfer, _, bitrate, t_unit, retr, role = experiment[0].strip().split()[2:]
    result[role] = {"transfer": float(transfer), "bitrate": float(bitrate) if 'M' in t_unit else float(bitrate) / 1000,
                    "retr": int(retr)}

    interval, _, transfer, _, bitrate, t_unit, role = experiment[1].strip().split()[2:]
    result[role] = {"transfer": float(transfer), "bitrate": float(bitrate) if 'M' in t_unit else float(bitrate) / 1000,
                    "retr": int(retr)}

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


def plot_latency_bitrate_figure(live_live_results, baseline_results):
    def plot_latency_bitrate_line(res, role, color, marker, label, errorbar_color):
        to_plot = {'x': [], 'y': [], 'dy': []}
        for delay, results in sorted(res.items(), key=lambda item: item[0]):
            values = []
            for result in results:
                if result:
                    values.append(result[role]['bitrate'])

            to_plot['x'].append(delay)
            to_plot['y'].append(statistics.mean(values))
            to_plot['dy'].append(statistics.stdev(values))

        plt.plot(to_plot['x'], to_plot['y'], label=label, linestyle='dashed', fillstyle='none', color=color,
                 marker=marker)
        print(label, to_plot)
        for idx, x in enumerate(to_plot['x']):
            plt.errorbar(x, to_plot['y'][idx], yerr=to_plot['dy'][idx], color=errorbar_color, elinewidth=1, capsize=1)

    plt.clf()
    ax = plt.gca()
    plot_latency_bitrate_line(live_live_results, "receiver", 'red', "o", "LiveLive-Receiver", "darkred")
    # plot_latency_bitrate_line(live_live_results, "sender", 'orange', "o", "LiveLive-Sender", "darkorange")

    plot_latency_bitrate_line(baseline_results, "receiver", 'royalblue', "^", "Baseline-Receiver", "blue")
    # plot_latency_bitrate_line(baseline_results, "sender", 'green', "^", "Baseline-Sender", "darkgreen")

    plt.xlabel('Latency [ms]')
    plt.ylabel('Bitrate [Mbps]')
    plt.legend(loc=(0.65, 0.55), labelspacing=0.2, prop={'size': 8})
    plt.savefig(os.path.join(FIGURES_PATH, "latency_bitrate.pdf"), format="pdf", bbox_inches='tight')


def plot_latency_retry_figure(live_live_results, baseline_results):
    def plot_latency_retry_line(res, role, color, marker, label, errorbar_color):
        to_plot = {'x': [], 'y': [], 'dy': []}
        for delay, results in sorted(res.items(), key=lambda item: item[0]):
            values = []
            for result in results:
                if result:
                    values.append(result[role]['retr'])

            to_plot['x'].append(delay)
            to_plot['y'].append(statistics.mean(values))
            to_plot['dy'].append(statistics.stdev(values))

        plt.plot(to_plot['x'], to_plot['y'], label=label, linestyle='dashed', fillstyle='none', color=color,
                 marker=marker)

        for idx, x in enumerate(to_plot['x']):
            plt.errorbar(x, to_plot['y'][idx], yerr=to_plot['dy'][idx], color=errorbar_color, elinewidth=1, capsize=1)

    plt.clf()
    ax = plt.gca()
    plot_latency_retry_line(live_live_results, "receiver", 'red', "o", "LiveLive-Receiver", "darkred")
    # plot_latency_bitrate_figure_line(live_live_results, "sender", 'orange', "o", "LiveLive-Sender", "darkorange")

    plot_latency_retry_line(baseline_results, "receiver", 'royalblue', "^", "Baseline-Receiver", "blue")
    # plot_latency_bitrate_figure_line(baseline_results, "sender", 'green', "^", "Baseline-Sender", "darkgreen")

    plt.xlabel('Latency [ms]')
    plt.ylabel('N. TCP Retransmissions')
    plt.legend(loc=(0.65, 0.55), labelspacing=0.2, prop={'size': 8})
    plt.savefig(os.path.join(FIGURES_PATH, "latency_retransmission.pdf"), format="pdf", bbox_inches='tight')


if __name__ == '__main__':
    os.makedirs(FIGURES_PATH, exist_ok=True)

    plt.figure(figsize=(4, 2))
    matplotlib.rc('font', size=8)
    matplotlib.rcParams['hatch.linewidth'] = 0.3
    matplotlib.rcParams['pdf.fonttype'] = 42
    matplotlib.rcParams['ps.fonttype'] = 42

    ll_results = parse_iperf_experiments("results/live-live/")
    baseline_res = parse_iperf_experiments("results/baseline/")
    plot_latency_bitrate_figure(ll_results, baseline_res)
    plot_latency_retry_figure(ll_results, baseline_res)
