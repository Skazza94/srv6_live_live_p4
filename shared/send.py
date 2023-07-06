#!/usr/bin/env python
import argparse
import sys
import random
import struct

from scapy.all import sendp, srploop, get_if_hwaddr, bind_layers
from scapy.all import Packet, Ether, IPv6, UDP, TCP, Raw, ShortField


class LiveLiveSeqN(Packet):
    name = "LiveLiveSeqN "
    fields_desc = [ShortField("seq_n", 0xffff)]

    def mysummary(self):
        return self.sprintf("LiveLiveSeqN %LiveLiveSeqN.seq_n%")
    

bind_layers(UDP, LiveLiveSeqN)


def main():
    if len(sys.argv) < 3:
        print('Usage: send.py <destination> <iface>')
        exit(1)

    addr = sys.argv[1]
    iface = sys.argv[2]

    pkt = Ether(src=get_if_hwaddr(iface), dst='00:00:00:e1:0a:00')
    pkt = pkt / IPv6(dst=addr) / UDP(dport=random.randint(5000, 60000), sport=random.randint(49152, 65535)) / LiveLiveSeqN(seq_n=0xffff)
    srploop(pkt, iface=iface, inter=0.1)


if __name__ == '__main__':
    main()
