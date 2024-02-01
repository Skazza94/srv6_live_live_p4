import json
import os
import statistics
import sys

import matplotlib
import matplotlib.pyplot as plt

import utils

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


def parse_simple_iperf_experiments(directory):
    results = {}

    for test_type in filter(lambda i: not i.startswith("."), os.listdir(directory)):
        results[test_type] = {}
        type_path = os.path.join(directory, test_type)
        for metric in filter(lambda i: not i.startswith("."), os.listdir(type_path)):
            results[test_type][metric] = {}
            metric_path = os.path.join(type_path, metric)
            for val in sorted(map(lambda i: int(i), filter(lambda i: not i.startswith("."), os.listdir(metric_path)))):
                val_folder = os.path.join(metric_path, str(val))
                results[test_type][metric][val] = []
                for experiment in sorted(filter(lambda i: not i.startswith("."), os.listdir(val_folder))):
                    experiment_path = os.path.join(val_folder, experiment)
                    results[test_type][metric][val].append(parse_iperf_experiment(experiment_path))

    return results


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


def plot_latency_bitrate_figure(results):
    def plot_latency_bitrate_line(type_results, color, marker, label, errorbar_color):
        to_plot = {'x': [], 'y': [], 'dy': []}
        for delay, type_result in sorted(type_results.items(), key=lambda item: item[0]):
            values = []
            for result in type_result:
                if result:
                    values.append(result['bits_per_second'] / 1000000)

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
    plot_latency_bitrate_line(results['live-live']['loss'], 'red', "o", "LiveLive", "darkred")
    plot_latency_bitrate_line(results['baseline']['loss'], 'blue', "^", "Baseline", "blue")

    plt.xlabel('Packet Loss [%]')
    plt.ylabel('Bitrate [Mbps]')
    plt.legend(loc=(0.65, 0.55), labelspacing=0.2, prop={'size': 8})
    plt.savefig(os.path.join(figures_path, "latency_bitrate.pdf"), format="pdf", bbox_inches='tight')


def plot_latency_retry_figure(results):
    def plot_latency_retry_line(type_results, color, marker, label, errorbar_color):
        to_plot = {'x': [], 'y': [], 'dy': []}
        for delay, type_result in sorted(type_results.items(), key=lambda item: item[0]):
            values = []
            for result in type_result:
                if result:
                    values.append(result['retr'])

            to_plot['x'].append(delay)
            to_plot['y'].append(statistics.mean(values))
            to_plot['dy'].append(statistics.stdev(values))

        plt.plot(to_plot['x'], to_plot['y'], label=label, linestyle='dashed', fillstyle='none', color=color,
                 marker=marker)
        for idx, x in enumerate(to_plot['x']):
            plt.errorbar(x, to_plot['y'][idx], yerr=to_plot['dy'][idx], color=errorbar_color, elinewidth=1, capsize=1)

    plt.clf()
    ax = plt.gca()
    plot_latency_retry_line(results['live-live']['loss'], 'red', "o", "LiveLive", "darkred")
    plot_latency_retry_line(results['baseline']['loss'], 'blue', "^", "Baseline", "blue")

    plt.xlabel('Packet Loss [%]')
    plt.ylabel('N. TCP Retransmissions')
    plt.legend(loc=(0.65, 0.55), labelspacing=0.2, prop={'size': 8})
    plt.savefig(os.path.join(figures_path, "latency_retransmission.pdf"), format="pdf", bbox_inches='tight')


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
        plt.plot(
            to_plot['x'], to_plot['y'], label=label, linestyle='dashed', fillstyle='none', color=color,
            marker=marker
        )
        for idx, x in enumerate(to_plot['x']):
            plt.errorbar(x, to_plot['y'][idx], yerr=to_plot['dy'][idx], color=errorbar_color, elinewidth=1, capsize=1)

    plt.clf()
    plot_n_path_bitrate_line(results['single'], 'green', "v", "Single", "darkgreen")
    plot_n_path_bitrate_line(results['random'], 'blue', "^", "Random", "darkblue")
    plot_n_path_bitrate_line(results['no-deduplicate'], 'goldenrod', "s", "No Deduplication", "darkgoldenrod")
    plot_n_path_bitrate_line(results['live-live'], 'red', "o", "LiveLive", "darkred")

    plt.xticks([2, 3, 4, 5, 6])

    plt.xlabel('N. Paths')
    plt.ylabel('Bitrate [Mbps]')
    plt.legend(loc='upper center', bbox_to_anchor=(0.5, 1.28), labelspacing=0.2, ncols=2, prop={'size': 8})
    plt.savefig(
        os.path.join(figures_path, f"n_path_bitrate.pdf"), format="pdf", bbox_inches='tight'
    )


def plot_n_path_cwd_figure(results):
    def plot_n_path_cwd_line(res, color, marker, label, errorbar_color):
        to_plot = {'x': [], 'y': [], 'dy': []}
        for n_path, res in sorted(res.items(), key=lambda item: item[0]):
            values = []
            for result in res:
                if label == "LiveLive":
                    print(n_path, label, result['cwd'] / 1000)

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
    plot_n_path_cwd_line(results['no-deduplicate'], 'goldenrod', "s", "No Deduplication", "darkgoldenrod")
    plot_n_path_cwd_line(results['live-live'], 'red', "o", "LiveLive", "darkred")

    plt.xticks([2, 3, 4, 5, 6])

    plt.xlabel('N. Paths')
    plt.ylabel('Cwnd Size [KBytes]')
    plt.legend(loc='upper center', bbox_to_anchor=(0.5, 1.28), labelspacing=0.2, ncols=2, prop={'size': 8})
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
    plot_n_path_retry_line(results['no-deduplicate'], 'goldenrod', "s", "No Deduplication", "darkgoldenrod")
    plot_n_path_retry_line(results['live-live'], 'red', "o", "LiveLive", "darkred")

    plt.legend(loc='upper center', bbox_to_anchor=(0.5, 1.28), labelspacing=0.2, ncols=2, prop={'size': 8})

    plt.xticks([2, 3, 4, 5, 6])

    plt.xlabel('N. Paths')
    plt.ylabel('N. TCP Retransmissions')
    plt.legend(loc='upper center', bbox_to_anchor=(0.5, 1.28), labelspacing=0.2, ncols=2, prop={'size': 8})
    plt.savefig(
        os.path.join(figures_path, f"n_path_retr.pdf"), format="pdf", bbox_inches='tight'
    )


def plot_concurrent_flows_fct_figure(results_path: str):
    def plot_concurrent_flows_line(experiment_type: str, color: str, marker: str):
        to_plot = {'x': [], 'y': [], 'dy': []}
        path = os.path.join(results_path, experiment_type)
        for number_of_flows in sorted(os.listdir(path), key=lambda x: int(x)):
            if number_of_flows == "0" or number_of_flows == "100":
                continue
            print(experiment_type, number_of_flows)
            test_path = os.path.join(path, number_of_flows)
            ll_flow_path = os.path.join(test_path, "concurrent-flows")
            test_seconds = []
            for filename in sorted(os.listdir(ll_flow_path)):
                file_path = os.path.join(ll_flow_path, filename)
                with open(file_path, "r") as stats_file:
                    stats = json.load(stats_file)
                for stat in stats:
                    if stat and 'end' in stat and 'streams' in stat['end']:
                        test_seconds.append(stat['end']['streams'][0]['sender']['seconds'])
            to_plot['x'].append(number_of_flows)
            to_plot['y'].append(statistics.mean(test_seconds))
            to_plot['dy'].append(statistics.stdev(test_seconds))
        plt.plot(
            to_plot['x'], to_plot['y'], label=experiment_type, linestyle='dashed', fillstyle='none', color=color,
            marker=marker
        )

    plt.clf()

    plot_concurrent_flows_line("live-live", "green", "o")
    plot_concurrent_flows_line("baseline", "blue", "x")

    plt.legend(loc='upper center', bbox_to_anchor=(0.5, 1.28), labelspacing=0.2, ncols=2, prop={'size': 8})

    plt.xlabel('N. Flows')
    plt.ylabel('FCT (seconds)')
    plt.legend(loc='upper center', bbox_to_anchor=(0.5, 1.28), labelspacing=0.2, ncols=2, prop={'size': 8})
    plt.savefig(
        os.path.join(figures_path, f"fct.pdf"), format="pdf", bbox_inches='tight'
    )


def plot_live_live_fct_figure(results_path: str):
    def plot_ll_line(experiment_type, color: str, marker: str):
        path = os.path.join(results_path, experiment_type)
        to_plot = {'x': [], 'y': [], 'dy': []}
        for number_of_flows in sorted(os.listdir(path), key=lambda x: int(x)):
            test_path = os.path.join(path, number_of_flows)
            ll_flow_path = os.path.join(test_path, "ll-flow")
            test_seconds = []
            for filename in sorted(os.listdir(ll_flow_path)):
                file_path = os.path.join(ll_flow_path, filename)
                with open(file_path, "r") as stats_file:
                    stats = json.load(stats_file)
                test_seconds.append(stats['end']['streams'][0]['sender']['seconds'])
            to_plot['x'].append(number_of_flows)
            to_plot['y'].append(statistics.mean(test_seconds))
            to_plot['dy'].append(statistics.stdev(test_seconds))
        plt.plot(
            to_plot['x'], to_plot['y'], label=experiment_type, linestyle='dashed', fillstyle='none', color=color,
            marker=marker
        )

    plt.clf()

    plot_ll_line("live-live", "green", "o")
    plot_ll_line("baseline", "blue", "x")

    plt.legend(loc='upper center', bbox_to_anchor=(0.5, 1.28), labelspacing=0.2, ncols=2, prop={'size': 8})

    plt.xlabel('N. Flows')
    plt.ylabel('FCT (seconds)')
    plt.legend(loc='upper center', bbox_to_anchor=(0.5, 1.28), labelspacing=0.2, ncols=2, prop={'size': 8})
    plt.savefig(
        os.path.join(figures_path, f"live_live_fct.pdf"), format="pdf", bbox_inches='tight'
    )


def plot_live_live_seqn_figure(results_path: str):
    def plot_seq_n_line(number_of_flows: str):
        to_plot_1 = {'x': [], 'y': [], 'dy': []}
        to_plot_2 = {'x': [], 'y': [], 'dy': []}
        print(results_path)
        experiment_path = os.path.join(results_path, "live-live", number_of_flows, "pcaps", "1")
        eth0_seq_n = utils.filter_points(utils.extract_ll_seqn(os.path.join(experiment_path, "e20.pcap")), 100)
        eth1_seq_n = utils.filter_points(utils.extract_ll_seqn(os.path.join(experiment_path, "e21.pcap")), 100)
        to_plot_1['x'] = list(map(lambda x: x[0], eth0_seq_n))
        to_plot_1['y'] = list(map(lambda x: x[1], eth0_seq_n))
        plt.plot(
            to_plot_1['x'], to_plot_1['y'], label="active", linestyle='dashed', fillstyle='none', color="green",
            marker=None
        )

        to_plot_2['x'] = list(map(lambda x: x[0], eth1_seq_n))
        to_plot_2['y'] = list(map(lambda x: x[1], eth1_seq_n))
        plt.plot(
            to_plot_2['x'], to_plot_2['y'], label="backup", linestyle='dashed', fillstyle='none', color="blue",
            marker=None
        )


    plt.clf()

    plot_seq_n_line("10")

    plt.legend(loc='upper center', bbox_to_anchor=(0.5, 1.28), labelspacing=0.2, ncols=2, prop={'size': 8})

    plt.xlabel('Time (seconds)')
    plt.ylabel('Sequence Number')
    plt.legend(loc='upper center', bbox_to_anchor=(0.5, 1.28), labelspacing=0.2, ncols=2, prop={'size': 8})
    plt.savefig(
        os.path.join(figures_path, f"live_live_seqn.pdf"), format="pdf", bbox_inches='tight'
    )


if __name__ == '__main__':
    if len(sys.argv) != 5:
        print(
            "Usage: plot.py <simple_results> <n_path_results> <multiple_flows_results> <figures_path>"
        )
        exit(1)

    simple_results_path = os.path.abspath(sys.argv[1])
    n_path_results_path = os.path.abspath(sys.argv[2])
    flows_results_path = os.path.abspath(sys.argv[3])
    figures_path = os.path.abspath(sys.argv[4])

    os.makedirs(figures_path, exist_ok=True)

    plt.figure(figsize=(4, 2))
    matplotlib.rc('font', size=8)
    matplotlib.rcParams['hatch.linewidth'] = 0.3
    matplotlib.rcParams['pdf.fonttype'] = 42
    matplotlib.rcParams['ps.fonttype'] = 42

    # simple_results = parse_simple_iperf_experiments(simple_results_path)
    # n_path_results = parse_n_path_iperf_experiments(n_path_results_path)
    # plot_latency_bitrate_figure(simple_results)
    # plot_latency_retry_figure(simple_results)

    plot_concurrent_flows_fct_figure(flows_results_path)
    plot_live_live_fct_figure(flows_results_path)
    plot_live_live_seqn_figure(flows_results_path)

    # plot_n_path_bitrate_figure(n_path_results)
    # plot_n_path_rtt_figure(n_path_results)
    # plot_n_path_cwd_figure(n_path_results)
    # plot_n_path_retry_figure(n_path_results)
