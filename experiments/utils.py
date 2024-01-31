from scapy.all import rdpcap, raw
import struct

from scapy.layers.inet6 import IPv6


def extract_ll_seqn(path: str):
    sequence_numbers = []
    first_time = None
    for pkt in rdpcap(path):
        if pkt.haslayer(IPv6) and pkt[IPv6].dst == "e2::55":
            if not first_time:
                first_time = pkt.time
            seq_n = struct.unpack(">H", raw(pkt)[96:98])[0]
            sequence_numbers.append((pkt.time - first_time, seq_n))
    return sequence_numbers


def filter_points(points: list, step: int):
    filtered_points = []
    for i, v in enumerate(points):
        if i % step == 0:
            filtered_points.append(v)
    return filtered_points


if __name__ == '__main__':
    extract_ll_seqn("../results/live-live/delay/10/pcaps/1/e20.pcap")
