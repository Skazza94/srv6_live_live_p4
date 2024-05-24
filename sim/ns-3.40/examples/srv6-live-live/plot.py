import json
import os
import ipaddress
import statistics
import sys
from datetime import datetime

import matplotlib
import matplotlib.pyplot as plt
import numpy as np
from itertools import islice
from sortedcontainers import SortedDict

figures_path = "figures"

def parse_data_file(file_path):
    parsed_result = {'x': [], 'y': []}
    with open(file_path, "r") as cwnd_file:
        lines = cwnd_file.readlines()
    
    for line in lines:
        line = line.strip().split(" ")
        parsed_result['x'].append(float(line[0]))
        parsed_result['y'].append(float(line[1]))

    return parsed_result


def closest(sorted_dict, key):
    assert len(sorted_dict) > 0
    keys = list(islice(sorted_dict.irange(minimum=key), 1))
    keys.extend(islice(sorted_dict.irange(maximum=key, reverse=True), 1))
    return min(keys, key=lambda k: abs(key - k))


def plot_cwnd_figure(results):
    cwnd_results_path = os.path.join(results, "cwnd")

    def plot_cwnd_line(node_type, color, errorbar_color, marker, label, end_x=None):
        for file_name in sorted(os.listdir(cwnd_results_path)):
            if node_type not in file_name:
                continue
            to_plot = parse_data_file(os.path.join(cwnd_results_path, file_name))

            if end_x: 
                to_plot['x'].append(end_x)
                to_plot['y'].append(to_plot['y'][-1])
            
            plt.plot(to_plot['x'], to_plot['y'], label=file_name.replace(".data", ""), 
                     linestyle="dashed", fillstyle='none', color=color, marker=marker)
            return to_plot['x']
            
    plt.clf()
    x_values = plot_cwnd_line("ll", 'blue', "darkblue", None, "Live-Live")
    plot_cwnd_line("active", 'red', "darkred", None, "Active")
    plot_cwnd_line("backup", 'green', "darkgreen", None, "Backup")


    # plt.xticks([0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10])

    plt.xlabel('Time (s)')
    plt.ylabel('CWND Size')
    plt.legend(loc='upper center', bbox_to_anchor=(0.5, 1.2), labelspacing=0.2, ncols=3, prop={'size': 8})
    experiment_name = "-".join(results.split("/")[-7:])
    plt.savefig(
        os.path.join(figures_path, f"cwnd_figure_{experiment_name}.pdf"), format="pdf", bbox_inches='tight'
    )


def plot_throughput_figure(results):
    params = results.split('/')[-7:]
    (_, n_active_flows, n_backup_flows, exp_type) = params[0].split('-')
    n_active_flows = int(n_active_flows)
    n_backup_flows = int(n_backup_flows)
    is_random = exp_type == "r"

    def closest(sorted_dict, key):
        assert len(sorted_dict) > 0
        keys = list(islice(sorted_dict.irange(minimum=key), 1))
        keys.extend(islice(sorted_dict.irange(maximum=key, reverse=True), 1))
        return min(keys, key=lambda k: abs(key - k))

    tp_results_path = os.path.join(results, "throughput")

    def plot_throughput_line(node_type, color, errorbar_color, marker, label):
        for file_name in os.listdir(tp_results_path):
            if node_type not in file_name:
                continue
            
            to_plot = parse_data_file(os.path.join(tp_results_path, file_name))

        plt.plot(to_plot['x'], [y / 1000000 for y in to_plot['y']], label=label, 
                linestyle="dashed", fillstyle='none', color=color, marker=marker)

    def plot_throughput_line_merge(node_type, color, errorbar_color, marker, label):
        to_plot_type = SortedDict({round(x, 1): [] for x in np.arange(0, 10.1, 0.5)})

        for file_name in os.listdir(tp_results_path):
            if node_type not in file_name:
                continue
            
            to_plot_file = SortedDict({round(x, 1): 0 for x in np.arange(0, 10.1, 0.5)})
            to_plot = parse_data_file(os.path.join(tp_results_path, file_name))

            for idx, t in enumerate(to_plot['x']):
                r_t = closest(to_plot_file, round(t, 1))

                if to_plot_file[r_t] == 0:
                    to_plot_file[r_t] = to_plot['y'][idx]
                else:
                    to_plot_file[r_t] = (to_plot_file[r_t] + to_plot['y'][idx]) / 2
            
            for t, val in to_plot_file.items():
                to_plot_type[t].append(val)

        to_plot_filtered = {}
        for t, vals in to_plot_type.items():
            to_plot_filtered[t] = sum(vals)
            
        plt.plot(to_plot_filtered.keys(), [y / 1000000 for y in to_plot_filtered.values()], label=label, 
                    linestyle="dashed", fillstyle='none', color=color, marker=marker)
            
    plt.clf()
    plt.yscale("log")

    plot_throughput_line("ll", 'blue', "darkblue", None, "Live-Live")
    plot_throughput_line_merge("active", 'orange', "darkgreen", None, "Active")
    plot_throughput_line_merge("backup", 'red', "darkred", None, "Backup")

    plt.ylim(bottom=0)
    plt.xlabel('Time [s]')
    plt.ylabel('Throughput [Mbps]')
    plt.legend(loc='upper center', bbox_to_anchor=(0.5, 1.2), labelspacing=0.2, ncols=3, prop={'size': 8})
    experiment_name = "-".join(results.split("/")[-7:])
    plt.savefig(
        os.path.join(figures_path, f"tp_figure_{experiment_name}.pdf"), format="pdf", bbox_inches='tight'
    )

# def plot_fct_figure(results):
#     flow_results_path = os.path.join(results, "flows_results.json")
#     with open(flow_results_path, "r") as flow_results_file: 
#         flow_results = json.load(flow_results_file)

#     def plot_fct_line(network, color, errorbar_color, marker, label):
#         to_plot = {'x': [], 'y': []}
#         network = ipaddress.ip_network(network, strict=False)
#         for flow_id, flow in flow_results['flows'].items():
#             ip = ipaddress.ip_address(flow['src_addr'])
#             if ip in network:
#                 pass
            
#     plt.clf()
#     plot_fct_line("ll", 'blue', "darkblue", None, "Live-Live")
#     plot_fct_line("backup", 'green', "darkgreen", None, "Backup")
#     plot_fct_line("active", 'red', "darkred", None, "Active")

#     # plt.xticks([0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10])

#     plt.xlabel('Time (s)')
#     plt.ylabel('CWND Size')
#     plt.legend(loc='upper center', bbox_to_anchor=(0.5, 1.2), labelspacing=0.2, ncols=3, prop={'size': 8})
#     experiment_name = "-".join(results.split("/")[-5:])
#     plt.savefig(
#         os.path.join(figures_path, f"cwnd_figure_{experiment_name}.pdf"), format="pdf", bbox_inches='tight'
#     )

def plot_seqn_figure(results):
    def plot_seqn_line(ll_port, color, marker, label, errorbar_color):
        to_plot = {'x': [], 'y': [], 'dy': []}
        with open(os.path.join(results, "log.txt"), "r") as f:
            seqn_lines = f.readlines()

        starting_ts = None
        for line in seqn_lines:
            if not "ll-pkt-seqno" in line:
                continue
            line = line.strip().split()
            port = int(line[-1])
            if port == ll_port:
                ts = datetime.fromtimestamp(int(line[6])/10**9).timestamp()
                if not starting_ts:
                    starting_ts = ts

                seqn = int(line[9])
                to_plot['x'].append(ts - starting_ts)
                to_plot['y'].append(seqn)
        plt.plot(to_plot['x'], to_plot['y'], label=label, linestyle="dashed", fillstyle='none', color=color,
                 marker=marker)

    plt.clf()
    plot_seqn_line(2, 'blue', None, "Backup", "darkblue")
    plot_seqn_line(1, 'red', None, "Active", "darkred")
    # plt.xticks([0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10])

    plt.xlabel('Time (s)')
    plt.ylabel('Sequence Number')
    plt.legend(loc='upper center', bbox_to_anchor=(0.5, 1.2), labelspacing=0.2, ncols=3, prop={'size': 8})
    experiment_name = "-".join(results.split("/")[-7:])
    plt.savefig(
        os.path.join(figures_path, f"seqn_figure_{experiment_name}.pdf"), format="pdf", bbox_inches='tight'
    )


def plot_sdwan_figure(results):
    params = results.split('/')[-7:]
    (n_active_flows, n_backup_flows, exp_type) = params[0].split('-')
    n_active_flows = int(n_active_flows)
    n_backup_flows = int(n_backup_flows)
    is_random = exp_type == "r"

    tp_results_path = os.path.join(results, "throughput")
    def plot_throughput_line(node_type, color, errorbar_color, marker, label):
        for file_name in os.listdir(tp_results_path):
            if node_type not in file_name:
                continue
            
            to_plot = parse_data_file(os.path.join(tp_results_path, file_name))

        return plt.plot(to_plot['x'][1:], [y / 1000000 for y in to_plot['y']][1:], label=label, 
                        linestyle="dashed", fillstyle='none', color=color, marker=marker)

    def plot_throughput_line_merge(node_type, color, errorbar_color, marker, label):
        to_plot_type = SortedDict({round(x, 1): [] for x in np.arange(1.0, 10.1, 0.5)})

        for file_name in os.listdir(tp_results_path):
            if node_type not in file_name:
                continue
            
            to_plot_file = SortedDict({round(x, 1): 0 for x in np.arange(1.0, 10.1, 0.5)})
            to_plot = parse_data_file(os.path.join(tp_results_path, file_name))

            for idx, t in enumerate(to_plot['x']):
                r_t = closest(to_plot_file, round(t, 1))

                if to_plot_file[r_t] == 0:
                    to_plot_file[r_t] = to_plot['y'][idx]
                else:
                    to_plot_file[r_t] = (to_plot_file[r_t] + to_plot['y'][idx]) / 2
            
            for t, val in to_plot_file.items():
                to_plot_type[t].append(val)

        to_plot_filtered = {}
        for t, vals in to_plot_type.items():
            to_plot_filtered[t] = sum(vals)
            
        return plt.plot(to_plot_filtered.keys(), [y / 1000000 for y in to_plot_filtered.values()], label=label, 
                        linestyle="dashed", fillstyle='none', color=color, marker=marker)
            
    with open(os.path.join(results, "log.txt"), "r") as f:
        lines = f.readlines()

    livelive_enable_ts = None
    to_plot_latency = {'x': [], 'y': []}
    for line in lines:
        if "ll-sdwan-enabled" not in line and "ll-latency-ts" not in line:
            continue

        if "ll-sdwan-enabled" in line and livelive_enable_ts is None:
            (_, value) = line.strip().split("=")
            livelive_enable_ts = float(value)
        elif "ll-latency-ts" in line:
            (ts_part, value_part) = line.strip().split()
            (_, ts_value) = ts_part.split("=")
            (_, value_value) = value_part.split("=")

            ts_value = float(ts_value)
            if ts_value < 1.0 or ts_value > 10.0:
                continue

            to_plot_latency['x'].append(ts_value)
            to_plot_latency['y'].append(int(value_value) / 1000)

    plt.clf()
    line1 = plot_throughput_line("ll", 'blue', "darkblue", None, "Throughput")
    
    line2 = plt.axvline(x=livelive_enable_ts, color='green', label="Alert")
    line3 = plt.axvline(x=4, color='red', label="Congestion")

    ax = plt.gca()
    ax2 = ax.twinx()
    ax2.set_ylabel('Latency [ms]')
    ax2.set_ylim([0, 6])
    ax2.set_yticks([0, 1, 2, 3, 4, 5, 6])
    line4 = ax2.plot(to_plot_latency['x'], to_plot_latency['y'], linestyle="dashed", fillstyle='none', color="black", label="Latency")

    lns = [line3, line2]
    labels = [l.get_label() for l in lns]
    ax.legend(lns, labels, loc='upper center', bbox_to_anchor=(0.5, 1.2), labelspacing=0.2, ncols=4, prop={'size': 8})

    plt.xticks([0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10])
    ax.set_ylim([0, 7])
    ax.set_yticks([0, 1, 2, 3, 4, 5, 6, 7])
    ax.set_xlabel('Time [s]')
    ax.set_ylabel('Throughput [Mbps]', color="blue")
    experiment_name = "-".join(results.split("/")[-7:])
    plt.savefig(
        os.path.join(figures_path, f"sdwan_figure_{experiment_name}.pdf"), format="pdf", bbox_inches='tight'
    )


if __name__ == '__main__':
    if len(sys.argv) != 3:
        print(
            "Usage: plot.py <results_path> <figures_path>"
        )
        exit(1)

    matplotlib.rc('font', size=10)
    matplotlib.rcParams['pdf.fonttype'] = 42
    matplotlib.rcParams['ps.fonttype'] = 42
    
    results_path = os.path.abspath(sys.argv[1])
    figures_path = os.path.abspath(sys.argv[2])

    print(f"Results Path: {results_path}")
    print(f"Figures Path: {figures_path}")

    os.makedirs(figures_path, exist_ok=True)

    plt.figure(figsize=(3.5, 2))

    plot_sdwan_figure(results_path)
