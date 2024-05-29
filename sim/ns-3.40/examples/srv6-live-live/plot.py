import os
import sys
from itertools import islice

import matplotlib
import matplotlib.patches as mpatches
import matplotlib.pyplot as plt
import numpy as np
from flowmon_parser import parse_xml, FiveTuple, Flow, Simulation
from sortedcontainers import SortedDict


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


def parse_data_file(file_path):
    parsed_result = {'x': [], 'y': []}
    with open(file_path, "r") as cwnd_file:
        lines = cwnd_file.readlines()

    for line in lines:
        line = line.strip().split(" ")
        # if float(line[0]) > 12:
        #     continue
        if parsed_result['x'] and float(line[0]) - parsed_result['x'][-1] < 0.1:
            continue 
        parsed_result['x'].append(float(line[0]))
        parsed_result['y'].append(float(line[1]))

    return parsed_result


def plot_cwnd_figure(results):
    cwnd_results_path = os.path.join(results, "cwnd")

    def plot_cwnd_line(node_type, color, marker, label):
        for file_name in sorted(os.listdir(cwnd_results_path)):
            if node_type not in file_name:
                continue
            to_plot = parse_data_file(os.path.join(cwnd_results_path, file_name))
            to_plot["y"] = [val/1000 for val in to_plot["y"]] 

            plt.plot(to_plot['x'], to_plot['y'], label=label,
                     linestyle="dashed", fillstyle='none', color=color, marker=marker)
            return to_plot['x']

    plt.clf()
    plt.grid(linestyle='--', linewidth=0.5)
    x_values = plot_cwnd_line("ll", 'blue', None, "Live-Live Flow")
    plot_cwnd_line("active", 'red', None, "TCP Flow (Path 1)")
    plot_cwnd_line("backup", 'green', None, "TCP Flow (Path 2)")

    plt.xlabel('Time [s]')
    plt.ylabel('CWnd Size [KB]')
    plt.yticks(range(0, 12))
    plt.legend(loc='upper center', bbox_to_anchor=(0.5, 1.2), labelspacing=0.2, ncols=3, prop={'size': 6})
    experiment_name = "-".join(results.split("/")[-7:])
    plt.savefig(
        os.path.join(figures_path, f"cwnd_figure_{experiment_name}.pdf"), format="pdf", bbox_inches='tight'
    )


def plot_tcp_retransmission_figure(results):
    cwnd_results_path = os.path.join(results, "retransmissions")

    def plot_retransmissions_line(node_type, color, marker, label, linestyle, end_x=None):
        for file_name in sorted(os.listdir(cwnd_results_path)):
            if node_type not in file_name:
                continue
            to_plot = parse_data_file(os.path.join(cwnd_results_path, file_name))

            
            to_plot["x"].insert(0, 1)
            to_plot["y"].insert(0, 0)
            to_plot["x"].append(12)
            to_plot["y"].append(to_plot["y"][-1])

            plt.plot(to_plot['x'], to_plot['y'], label=label,
                     linestyle=linestyle, fillstyle='none', color=color, marker=marker)
            return to_plot['x']

    plt.clf()
    plt.grid(linestyle='--', linewidth=0.5)
    x_values = plot_retransmissions_line("ll", 'red', None, "Live-Live Flow", "solid")
    plot_retransmissions_line("active", 'green', None, "TCP Flow (Path 1)", "dashed")
    plot_retransmissions_line("backup", 'blue', None, "TCP Flow (Path 2)", "dotted")

    plt.xlabel('Time [s]')
    plt.xticks(range(0, 13))
    plt.xlim([0, 13])

    plt.ylabel('N. TCP Retransmissions')
    plt.yticks(range(0, 200, 20))
    plt.legend(loc='upper center', bbox_to_anchor=(0.5, 1.2), labelspacing=0.2, ncols=3, prop={'size': 6})
    experiment_name = "-".join(results.split("/")[-7:])
    plt.savefig(
        os.path.join(figures_path, f"retransmissions_figure_{experiment_name}.pdf"), format="pdf", bbox_inches='tight'
    )

def plot_throughput_figure(results):
    def closest(sorted_dict, key):
        assert len(sorted_dict) > 0
        keys = list(islice(sorted_dict.irange(minimum=key), 1))
        keys.extend(islice(sorted_dict.irange(maximum=key, reverse=True), 1))
        return min(keys, key=lambda k: abs(key - k))

    cwnd_results_path = os.path.join(results, "throughput")

    def plot_throughput_line(node_type, color, marker, label, linestyle):
        for file_name in os.listdir(cwnd_results_path):
            if node_type not in file_name:
                continue

            to_plot = parse_data_file(os.path.join(cwnd_results_path, file_name))

            to_plot_x = [x for x in to_plot['x'] if x <= 12]
            to_plot_y = to_plot['y'][:len(to_plot_x)]

            plt.plot(to_plot_x, [y / 1000000 for y in to_plot_y], label=label,
                     linestyle=linestyle, fillstyle='none', color=color, marker=marker)

            break

    def plot_throughput_line_merge(node_type, color, marker, label, experiment_time):
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
            if t > 13:
                continue

            to_plot_filtered[t] = sum(vals)

        plt.plot(to_plot_filtered.keys(), [y / 1000000 for y in to_plot_filtered.values()], label=label,
                 linestyle="dashed", fillstyle='none', color=color, marker=marker)

    plt.clf()
    plt.grid(linestyle='--', linewidth=0.5)

    plot_throughput_line("ll", 'red', None, "Live-Live Flow", "solid")
    plot_throughput_line("active-fg", 'green', None, "TCP Flow (Path 1)", "dashed")
    plot_throughput_line("backup-fg", 'blue', None, "TCP Flow (Path 2)", "dashed")

    # plt.xticks(range(0, 13))
    # plt.xlim([0, 13])
    plt.ylim([0, 80])

    plt.xlabel('Time [s]')
    plt.ylabel('Throughput [Mbps]')
    plt.legend(loc='upper center', bbox_to_anchor=(0.5, 1.2), labelspacing=0.2, ncols=3, prop={'size': 6})
    experiment_name = "-".join(results.split("/")[-7:])
    plt.savefig(
        os.path.join(figures_path, f"tp_figure_{experiment_name}.pdf"), format="pdf", bbox_inches='tight'
    )


def plot_seqn_figure(results):
    def plot_seqn_line(ll_port, color, marker, label):
        to_plot = {'x': [], 'y': [], 'dy': []}
        with open(os.path.join(results, "log.txt"), "r") as f:
            seqn_lines = f.readlines()

        for line in seqn_lines:
            if not "ll-pkt-seqno" in line:
                continue
            line = line.strip().split()
            port = int(line[-1])
            if port == ll_port:
                ts = int(line[6]) / 10 ** 9
                seqn = int(line[9])

                if ts > 12:
                    continue

                to_plot['x'].append(ts)
                to_plot['y'].append(seqn)
        plt.plot(to_plot['x'], to_plot['y'], label=label, linestyle="dashed", fillstyle='none', color=color,
                 marker=marker)

    plt.clf()
    plt.grid(linestyle='--', linewidth=0.5)
    plot_seqn_line(1, 'orange', None, "Path 1")
    plot_seqn_line(2, 'purple', None, "Path 2")
    plt.xticks(range(0, 13))
    plt.yticks([0, 5000, 10000, 15000, 20000, 25000, 30000])
    plt.xlim([0, 13])
    plt.ylim([0, 30000])

    ax = plt.gca()

    ax.yaxis.set_major_formatter(OOMFormatter(3, "%d"))
    plt.xlabel('Time [s]')
    plt.ylabel('Live-Live Seq. No.')
    plt.legend(loc='upper center', bbox_to_anchor=(0.5, 1.2), labelspacing=0.2, ncols=3, prop={'size': 6})
    experiment_name = "-".join(results.split("/")[-7:])
    plt.savefig(
        os.path.join(figures_path, f"seqn_figure_{experiment_name}.pdf"), format="pdf", bbox_inches='tight'
    )


def plot_delay_histogram_figure(results, addresses):
    flow_monitor_path = os.path.join(results, "flow-monitor", "flow_monitor.xml")
    sim: Simulation = parse_xml(flow_monitor_path)[0]

    def plot_delay_histogram(axes, src_addr, label, color, hatch):
        axes.grid(linestyle='--', linewidth=0.5)

        to_plot = []
        for flow in sim.flows:
            flow: Flow = flow
            t: FiveTuple = flow.fiveTuple
            if t.sourceAddress == src_addr:
                for bin in flow.delayHistogram:
                    to_plot.extend([float(bin.get("start")) * 1000] * int(bin.get("count")))
                axes.hist(
                    to_plot, label=label,
                    fill=None, hatch=hatch, edgecolor=color,
                    rwidth=0.8,
                    bins=range(0, 125, 5)
                )
                axes.set_xlim([0, 125])
                axes.set_ylim([0.1, 100000])
                axes.set_ylabel('N. Packets')
                axes.set_yscale("log")

                axes.set_yticks([0.1, 100, 100000])

                break

    plt.clf()

    fig, axs = plt.subplots(len(addresses), 1, sharey="all", tight_layout=True, figsize=(4, 4))
    handles = []
    for ax_n, (address, label, color, hatch) in enumerate(addresses):
        plot_delay_histogram(axs[ax_n], address, label, color, hatch)
        handles.append(mpatches.Patch(fill=None, hatch=hatch, edgecolor=color, label=label))
    plt.xlabel('Delay [ms]')

    fig.legend(handles=handles, loc='upper center', bbox_to_anchor=(0.5, 1.04), ncol=len(handles), prop={'size': 6})

    experiment_name = "-".join(results.split("/")[-7:])
    plt.savefig(
        os.path.join(figures_path, f"delay_histogram_figure_{experiment_name}.pdf"), format="pdf", bbox_inches='tight'
    )


def plot_fct_histogram_figure(results, addresses):
    flow_monitor_path = os.path.join(results, "flow-monitor", "flow_monitor.xml")

    plt.clf()
    plt.grid(linestyle='--', linewidth=0.5)

    sim: Simulation = parse_xml(flow_monitor_path)[0]
    labels = []
    colors = []
    fcts = []
    i = 0
    for (address, label, color, hatch) in addresses:
        labels.append(label)
        colors.append(color)
        for flow in sim.flows:
            flow: Flow = flow
            t: FiveTuple = flow.fiveTuple
            if t.sourceAddress == address:
                plt.bar([i], [flow.fct], fill=None, hatch=hatch, edgecolor=color, )
                fcts.append(flow.fct)
                i += 1

    plt.xticks([0, 1, 2], labels=[x[1] for x in addresses], size=6)
    plt.ylabel('FCT [ms]')
    plt.yticks(range(0, 16, 2))

    experiment_name = "-".join(results.split("/")[-7:])
    plt.savefig(
        os.path.join(figures_path, f"fct_histogram_figure_{experiment_name}.pdf"), format="pdf", bbox_inches='tight'
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

    plt.figure(figsize=(3.5, 2))

    plot_seqn_figure(results_path)
    plot_cwnd_figure(results_path)
    plot_tcp_retransmission_figure(results_path)
    plot_throughput_figure(results_path)

    plot_fct_histogram_figure(
        results_path,
        [("2001::1", "Live-Live Flow", "red", "////"), ("2003::1", "TCP Flow 2 (Path 1)", "green", "\\\\\\\\"),
         ("2005::1", "TCP Flow (Path 2)", "blue", "xxxx")])

    plot_delay_histogram_figure(
        results_path,
        [("2001::1", "Live-Live Flow", "red", "////"), ("2003::1", "TCP Flow (Path 1)", "green", "\\\\\\\\"),
         ("2005::1", "TCP Flow (Path 2)", "blue", "xxxx")])
    