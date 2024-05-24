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
from flowmon_parser import parse_xml, FiveTuple, Flow, Simulation
import matplotlib.patches as mpatches

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
    (_, n_active_flows, n_backup_flows, exp_type) = params[0].split('-')
    n_active_flows = int(n_active_flows)
    n_backup_flows = int(n_backup_flows)
    is_random = exp_type == "r"

    def closest(sorted_dict, key):
        assert len(sorted_dict) > 0
        keys = list(islice(sorted_dict.irange(minimum=key), 1))
        keys.extend(islice(sorted_dict.irange(maximum=key, reverse=True), 1))
        return min(keys, key=lambda k: abs(key - k))

    cwnd_results_path = os.path.join(results, "throughput")

    def plot_throughput_line(node_type, color, errorbar_color, marker, label):
        for file_name in os.listdir(cwnd_results_path):
            if node_type not in file_name:
                continue
            
            to_plot = parse_data_file(os.path.join(cwnd_results_path, file_name))

        plt.plot(to_plot['x'], [y / 1000000 for y in to_plot['y']], label=label, 
                linestyle="dashed", fillstyle='none', color=color, marker=marker)

    def plot_throughput_line_merge(node_type, color, errorbar_color, marker, label, experiment_time):
        to_plot_type = SortedDict({round(x, 1): [] for x in np.arange(0, experiment_time, 0.5)})

        for file_name in os.listdir(cwnd_results_path):
            if node_type not in file_name:
                continue
            
            to_plot_file = SortedDict({round(x, 1): 0 for x in np.arange(0, experiment_time, 0.5)})
            to_plot = parse_data_file(os.path.join(cwnd_results_path, file_name))

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
    # plt.yscale("log")

    plot_throughput_line("ll", 'blue', "darkblue", None, "Live-Live")
    plot_throughput_line("active-fg", 'orange', "darkgreen", None, "Active-TCP")
    plot_throughput_line("backup-fg", 'red', "darkred", None, "Backup-TCP")
    plot_throughput_line_merge("active-bg", 'purple', "darkgreen", None, "Active", 15)
    plot_throughput_line_merge("backup-bg", 'green', "darkred", None, "Backup", 15)

    # plt.ylim(bottom=0)
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
        print(ll_port, len(to_plot['y']))
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


def plot_delay_histogram_figure(results, addresses):
    flow_monitor_path = os.path.join(results, "flow-monitor", "flow_monitor.xml")
    sim : Simulation = parse_xml(flow_monitor_path)[0]
    def plot_delay_histogram(axes, src_addr, label, color):
        to_plot = []
        packets_count = 0
        for flow in sim.flows:
            flow: Flow = flow
            t: FiveTuple = flow.fiveTuple
            if t.sourceAddress == src_addr:
                for bin in flow.delayHistogram:
                    # print(bin.get("start"), bin.get("count"), [float(bin.get("start"))]*int(bin.get("count")))
                    to_plot.extend([float(bin.get("start"))*1000]*int(bin.get("count")))
                    packets_count += int(bin.get("count"))
                axes.hist(to_plot, label=label, color=color)
                axes.set_xlabel('Delay (ms)')
                axes.set_xlim([0, 500])
                axes.set_ylabel('Count')
                print(label, packets_count)
        
    plt.clf()
    
    fig, axs = plt.subplots(1, len(addresses), sharey=True, tight_layout=True)
    handles = []
    for ax_n, (address, label, color) in enumerate(addresses):
        plot_delay_histogram(axs[ax_n], address, label, color)
        handles.append(mpatches.Patch(color=color, label=label))
    plt.xlabel('Delay (ms)')
    plt.ylabel('Count')

    fig.legend(handles=handles, loc='upper center', bbox_to_anchor=(0.5,1.1), ncol=len(handles))
   
    experiment_name = "-".join(results.split("/")[-7:])
    plt.savefig(
        os.path.join(figures_path, f"delay_histogram_figure_{experiment_name}.pdf"), format="pdf", bbox_inches='tight'
    )
   

if __name__ == '__main__':
    if len(sys.argv) != 3:
        print(
            "Usage: plot.py <results_path>"
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
    plot_delay_histogram_figure(
        results_path, 
        [("2001::1", "live-live", "red"), ("2003::1", "active", "green"), ("2005::1", "backup", "blue")])
    # plot_fct_figure(results_path)
