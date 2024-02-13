import json
import os
import sys
import time
from functools import partial
from multiprocessing import cpu_count
from multiprocessing.dummy import Pool

try:
    from xml.etree import cElementTree as ElementTree
except ImportError:
    from xml.etree import ElementTree


def parse_time_ns(tm):
    if tm.endswith('ns'):
        return float(tm[:-2])

    raise ValueError(tm)


class FlowTuple(object):
    __slots__ = ['src_addr', 'dst_addr', 'protocol', 'src_port', 'dst_port']

    def __init__(self, el):
        if el is not None:
            self.src_addr = el.get('sourceAddress')
            self.dst_addr = el.get('destinationAddress')
            self.src_port = int(el.get('sourcePort'))
            self.dst_port = int(el.get('destinationPort'))
            self.protocol = int(el.get('protocol'))

    def set_tuple(self, src_addr, dst_addr, protocol, src_port, dst_port):
        self.src_addr = src_addr
        self.dst_addr = dst_addr
        self.src_port = src_port
        self.dst_port = dst_port
        self.protocol = protocol

    def __eq__(self, other):
        if not isinstance(other, FlowTuple):
            return NotImplemented
        return self.src_addr == other.src_addr and self.dst_addr == other.dst_addr and \
            self.src_port == other.src_port and self.dst_port == other.dst_port and \
            self.protocol == other.protocol

    def __str__(self):
        return "(" + str(self.src_addr) + ", " + str(self.dst_addr) + ", " + \
            str(self.src_port) + ", " + str(self.dst_port) + ", " \
            + str(self.protocol) + ")"

    def __hash__(self):
        return self.src_addr.__hash__() + self.dst_addr.__hash__() \
            + self.src_port.__hash__() + self.dst_port.__hash__() \
            + self.protocol.__hash__()


class Histogram(object):
    __slots__ = ['bins']

    def __init__(self, el=None):
        self.bins = []
        if el:
            for hist_bin in el.findall('bin'):
                self.bins.append(
                    (float(hist_bin.get("start")), float(hist_bin.get("width")), int(hist_bin.get("count")))
                )


class Flow(object):
    __slots__ = ['flow_id', 'delay_mean', 'packet_loss_ratio', 'rx_bitrate', 'tx_bitrate',
                 'flow_tuple', 'packet_size_mean', 'probe_stats_unsorted',
                 'hop_count', 'flow_interruptions_histogram', 'rx_duration',
                 'fct', 'tx_bytes', 'tx_packets', 'rx_packets', 'rx_bytes', 'lost_packets', 'nodes', 'paths',
                 'first_tx_packet', 'last_rx_packet']

    def __init__(self, flow_el):
        self.flow_id = int(flow_el.get('flowId'))
        self.rx_packets = int(flow_el.get('rxPackets'))
        self.tx_packets = int(flow_el.get('txPackets'))
        tx_duration = float(
            parse_time_ns(flow_el.get('timeLastTxPacket')) - parse_time_ns(flow_el.get('timeFirstTxPacket'))
        ) * 1e-9
        rx_duration = float(
            parse_time_ns(flow_el.get('timeLastRxPacket')) - parse_time_ns(flow_el.get('timeFirstRxPacket'))
        ) * 1e-9
        fct = float(
            parse_time_ns(flow_el.get('timeLastRxPacket')) - parse_time_ns(flow_el.get('timeFirstTxPacket'))
        ) * 1e-9
        self.tx_bytes = int(flow_el.get('txBytes'))
        self.rx_bytes = int(flow_el.get('rxBytes'))
        self.rx_duration = rx_duration
        self.first_tx_packet = float(parse_time_ns(flow_el.get('timeFirstTxPacket'))) * 1e-9
        self.last_rx_packet = float(parse_time_ns(flow_el.get('timeLastRxPacket'))) * 1e-9
        if fct > 0:
            self.fct = fct
        else:
            self.fct = None

        self.probe_stats_unsorted = []

        if self.rx_packets:
            self.hop_count = float(flow_el.get('timesForwarded')) / self.rx_packets + 1
        else:
            self.hop_count = -1000

        if self.rx_packets:
            self.delay_mean = float(parse_time_ns(flow_el.get('delaySum'))) / self.rx_packets * 1e-9
            self.packet_size_mean = float(flow_el.get('rxBytes')) / self.rx_packets
        else:
            self.delay_mean = None
            self.packet_size_mean = None

        if rx_duration > 0:
            self.rx_bitrate = int(flow_el.get('rxBytes')) * 8 / rx_duration
        else:
            self.rx_bitrate = None

        if tx_duration > 0:
            self.tx_bitrate = int(flow_el.get('txBytes')) * 8 / tx_duration
        else:
            self.tx_bitrate = None

        self.lost_packets = float(flow_el.get('lostPackets'))

        if self.rx_packets == 0:
            self.packet_loss_ratio = None
        else:
            self.packet_loss_ratio = (self.lost_packets / (self.rx_packets + self.lost_packets))

        interrupt_hist_elem = flow_el.find("flowInterruptionsHistogram")
        if interrupt_hist_elem is None:
            self.flow_interruptions_histogram = None
        else:
            self.flow_interruptions_histogram = Histogram(interrupt_hist_elem)

        self.nodes = list(
            map(
                lambda x: int(x.get('id')),
                sorted(flow_el.findall("node"), key=lambda x: int(x.get('time')))
            )
        )

        self.paths = list(
            map(
                lambda x: int(x.get('port')),
                sorted(flow_el.findall("path"), key=lambda x: int(x.get('time')))
            )
        )


class ProbeFlowStats(object):
    __slots__ = ['probe_id', 'packets', 'n_bytes', 'delay_from_first_probe']

    def __init__(self, probe_id, packets, n_bytes, delay_from_first_probe_sum):
        self.probe_id = probe_id
        self.packets = packets
        self.n_bytes = n_bytes

        if self.packets > 0:
            self.delay_from_first_probe = delay_from_first_probe_sum / float(self.packets)
        else:
            self.delay_from_first_probe = 0


class Simulation(object):
    __slots__ = ['flows']

    def __init__(self, simulation_el):
        self.flows = {}

        flow_classifiers = simulation_el.findall("Ipv4FlowClassifier") + simulation_el.findall("Ipv6FlowClassifier")
        for flow_el in simulation_el.findall("FlowStats/Flow"):
            flow = Flow(flow_el)
            self.flows[flow.flow_id] = flow
            
        for flow_class in flow_classifiers:
            for flow_cls in flow_class.findall("Flow"):
                flow_id = int(flow_cls.get('flowId'))
                self.flows[flow_id].flow_tuple = FlowTuple(flow_cls)

        for probe_elem in simulation_el.findall("FlowProbes/FlowProbe"):
            probe_id = int(probe_elem.get('index'))
            for stats in probe_elem.findall("FlowStats"):
                flow_id = int(stats.get('flowId'))
                probe_stats = ProbeFlowStats(
                    probe_id, int(stats.get('packets')), int(stats.get('bytes')),
                    parse_time_ns(stats.get('delayFromFirstProbeSum'))
                )
                self.flows[flow_id].probe_stats_unsorted.append(probe_stats)


simulations = {}


def parse_file(path, file):
    parts = file.replace('.xml', '').split('-')

    mode = parts[1]
    n_paths = int(parts[2])
    n_primary_flows = int(parts[3])
    n_backup_flows = int(parts[4])

    if mode not in simulations:
        simulations[mode] = {}

    if n_paths not in simulations[mode]:
        simulations[mode][n_paths] = {}

    if n_primary_flows not in simulations[mode][n_paths]:
        simulations[mode][n_paths][n_primary_flows] = {}

    if n_backup_flows not in simulations[mode][n_paths][n_primary_flows]:
        simulations[mode][n_paths][n_primary_flows][n_backup_flows] = []
    
    full_path = os.path.join(path, file)
    print("Reading XML file:", full_path)
    with open(full_path) as xml_file:
        for event, elem in ElementTree.iterparse(xml_file, events=("start", "end")):
            if event == "end" and elem.tag == 'FlowMonitor':
                sim = Simulation(elem)
                simulations[mode][n_paths][n_primary_flows][n_backup_flows].append(sim)
                elem.clear()

    print("Done.")


def main(argv):
    if len(argv) < 2:
        print("A path is required.")
        exit(1)

    files_pool = Pool(cpu_count() * 2)
    files_pool.map(func=partial(parse_file, argv[1]),
                   iterable=filter(lambda x: '.xml' in x, os.listdir(os.path.abspath(argv[1])))
                   )

    results = {}
    for sim_mode, sims_n_paths in simulations.items():
        for sim_n_paths, sims_n_primary in sims_n_paths.items():
            for sim_n_primary, sims_n_backup in sims_n_primary.items():
                for sim_n_backup, runs in sims_n_backup.items():
                    if sim_mode not in results:
                        results[sim_mode] = {}

                    if sim_n_paths not in results[sim_mode]:
                        results[sim_mode][sim_n_paths] = {}

                    if sim_n_primary not in results[sim_mode][sim_n_paths]:
                        results[sim_mode][sim_n_paths][sim_n_primary] = {}

                    if sim_n_backup not in results[sim_mode][sim_n_paths][sim_n_primary]:
                        results[sim_mode][sim_n_paths][sim_n_primary][sim_n_backup] = []

                    for run in runs:
                        total_fct = 0
                        flow_count = 0
                        large_flow_total_fct = 0
                        large_flow_count = 0
                        large_flow_total_rx_bytes = 0
                        small_flow_total_fct = 0
                        small_flow_count = 0

                        total_lost_packets = 0
                        total_tx_packets = 0
                        total_rx_packets = 0
                        total_rx_bytes = 0
                        fcts = []
                        small_fcts = []
                        large_fcts = []
                        max_small_flow_id = 0
                        max_small_flow_fct = 0

                        reversed_tuples = {}

                        for (flow_id, flow) in run.flows.items():
                            print("Flow ID: %d - Tuple: %s" % (flow_id, flow.flow_tuple))

                            to_continue = False
                            reversed_tuple = FlowTuple(None)
                            reversed_tuple.set_tuple(
                                flow.flow_tuple.dst_addr, flow.flow_tuple.src_addr, flow.flow_tuple.protocol,
                                flow.flow_tuple.dst_port, flow.flow_tuple.src_port
                            )
                            if flow.fct is None or flow.tx_bitrate is None or flow.rx_bitrate is None:
                                if reversed_tuple in reversed_tuples:
                                    del reversed_tuples[reversed_tuple]

                                continue

                            if flow.tx_bytes > 0:
                                to_continue = True
                                if reversed_tuple in reversed_tuples:
                                    original_flow = reversed_tuples[reversed_tuple]
                                    total_fct += original_flow.fct
                                    fcts.append(original_flow.fct)

                                    total_tx_packets += original_flow.tx_packets
                                    total_rx_packets += original_flow.rx_packets
                                    total_rx_bytes += original_flow.rx_bytes
                                    total_lost_packets += original_flow.lost_packets
                                    if original_flow.tx_bytes > 10000000:
                                        large_flow_count += 1
                                        large_flow_total_fct += original_flow.fct
                                        large_flow_total_rx_bytes += original_flow.rx_bytes
                                        large_fcts.append(original_flow.fct)
                                    if original_flow.tx_bytes < 100000:
                                        small_flow_count += 1
                                        small_flow_total_fct += original_flow.fct
                                        small_fcts.append(original_flow.fct)
                                        if original_flow.fct > max_small_flow_fct:
                                            max_small_flow_id = original_flow.flow_id
                                            max_small_flow_fct = original_flow.fct

                                    t = original_flow.flow_tuple
                                    proto = {6: 'TCP', 17: 'UDP'}[t.protocol]
                                    print("FlowID: %i (%s %s/%s --> %s/%i)" %
                                        (original_flow.flow_id, proto, t.src_addr, t.src_port, t.dst_addr, t.dst_port)
                                        )
                                    print("\tTX bitrate: %.2f kbit/s" % (original_flow.tx_bitrate * 1e-3,))
                                    print("\tRX bitrate: %.2f kbit/s" % (original_flow.rx_bitrate * 1e-3,))
                                    print("\tMean Delay: %.2f ms" % (original_flow.delay_mean * 1e3,))
                                    print("\tPacket Loss Ratio: %.2f %%" % (flow.packet_loss_ratio * 100))
                                    print("\tFlow size: %i bytes, %i packets" % (
                                        original_flow.tx_bytes, original_flow.tx_packets))
                                    print("\tRx %i bytes, %i packets" % (original_flow.rx_bytes, original_flow.rx_packets))
                                    print("\tDevice Lost %i packets" % original_flow.lost_packets)
                                    print("\tReal Lost %i packets" % (original_flow.tx_packets - original_flow.rx_packets))
                                    print("\tFCT: %.4f" % original_flow.fct)

                            if flow.rx_packets != flow.tx_packets:
                                pass

                            if to_continue:
                                continue

                            reversed_tuples[flow.flow_tuple] = flow
                            flow_count += 1

                        fcts = sorted(fcts)
                        large_fcts = sorted(large_fcts)
                        small_fcts = sorted(small_fcts)

                        print("Number of flows: %d" % flow_count)
                        print("Number of large flows: %d" % large_flow_count)
                        print("Number of small flows: %d" % small_flow_count)
                        if flow_count == 0:
                            print("No flows")
                        else:
                            results[sim_mode][sim_n_paths][sim_n_primary][sim_n_backup].append({
                                'fct_50': (total_fct / flow_count), 
                                'fct_99': (fcts[int((len(fcts) * 99) / 100)]), 
                                'fct_999': (fcts[int((len(fcts) * 999) / 1000)])
                            })

                            print("Avg FCT: %.4f" % (total_fct / flow_count))
                            print("Flow 99-ile FCT: %.4f" % (fcts[int((len(fcts) * 99) / 100)]))
                            print("Flow 99.9-ile FCT: %.4f" % (fcts[int((len(fcts) * 999) / 1000)]))

                        if large_flow_count == 0:
                            print("No large flows")
                        else:
                            print("Large Flow Avg FCT: %.4f" % (large_flow_total_fct / large_flow_count))
                            print("Large Flow 99-ile FCT: %.4f" % (large_fcts[int((len(large_fcts) * 99) / 100)]))
                            print("Large Flow 99.9-ile FCT: %.4f" % (large_fcts[int((len(large_fcts) * 999) / 1000)]))
                            print("Total Large RX Bytes: %.4f" % large_flow_total_rx_bytes)

                        if small_flow_count == 0:
                            print("No small flows")
                        else:
                            print("Small Flow Avg FCT: %.4f" % (small_flow_total_fct / small_flow_count))
                            print("Small Flow 99-ile FCT: %.4f" % (small_fcts[int((len(small_fcts) * 99) / 100)]))
                            print("Small Flow 99.9-ile FCT: %.4f" % (small_fcts[int((len(small_fcts) * 999) / 1000)]))

                        print("Total TX Packets: %i" % total_tx_packets)
                        print("Total RX Packets: %i" % total_rx_packets)
                        print("Total RX Bytes: %i" % total_rx_bytes)
                        print("Total Lost Packets: %i" % total_lost_packets)
                        print("Max Small flow Id: %i" % max_small_flow_id)

    results_file = 'results-%d.json' % time.time()
    with open(results_file, 'w') as results_file:
        results_file.write(json.dumps(results))

    print("Results saved in file: %s" % results_file)


if __name__ == '__main__':
    main(sys.argv)
