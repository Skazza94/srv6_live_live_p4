from scapy.all import rdpcap, raw
import struct

from scapy.layers.inet6 import IPv6


def extract_ll_seqn(path: str):
    for pkt in rdpcap(path):
        if pkt.haslayer(IPv6) and pkt[IPv6].dst == "e2::55":
            seq_n = struct.unpack(">H", raw(pkt)[96:98])[0]
            print(seq_n)


if __name__ == '__main__':
    extract_ll_seqn("../results/live-live/delay/10/pcaps/1/e20.pcap")
