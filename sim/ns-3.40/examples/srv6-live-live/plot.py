import json
import os
import ipaddress
import statistics
import sys
from datetime import datetime

import matplotlib
import matplotlib.pyplot as plt
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
    (_, n_active_flows, n_backup_flows) = params[0].split('-')
    n_active_flows = int(n_active_flows)
    n_backup_flows = int(n_backup_flows)

    cwnd_results_path = os.path.join(results, "throughput")

    def plot_throughput_line(node_type, color, errorbar_color, marker, label, single_file, expected_vals=0):
        to_plot_type = SortedDict()

        for file_name in os.listdir(cwnd_results_path):
            if node_type not in file_name:
                continue
            
            to_plot = parse_data_file(os.path.join(cwnd_results_path, file_name))

            for idx, t in enumerate(to_plot['x']):
                r_t = round(t, 1) if not single_file else t

                if r_t not in to_plot_type:
                    to_plot_type[r_t] = []
                to_plot_type[r_t].append(to_plot['y'][idx])

        to_plot_filtered = {}
        for t, vals in to_plot_type.items():
            if single_file:
                to_plot_filtered[t] = vals[0]
            else:
                if len(vals) == expected_vals:
                    to_plot_filtered[t] = sum(vals)
            
        plt.plot(to_plot_filtered.keys(), [y / 1000000 for y in to_plot_filtered.values()], label=label, 
                    linestyle="dashed", fillstyle='none', color=color, marker=marker)
            
    plt.clf()
    plot_throughput_line("ll", 'blue', "darkblue", None, "Live-Live", True)
    plot_throughput_line("active-bg", 'orange', "darkgreen", None, "Active BG", False, n_active_flows - 1)
    plot_throughput_line("active-fg", 'green', "darkgreen", None, "Active", True)
    plot_throughput_line("backup-bg", 'red', "darkred", None, "Backup BG", False, n_backup_flows - 1)
    plot_throughput_line("backup-fg", 'gold', "darkred", None, "Backup", True)

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

    plot_seqn_figure(results_path)
    plot_cwnd_figure(results_path)
    plot_throughput_figure(results_path)
    # plot_fct_figure(results_path)
