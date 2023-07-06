#!/usr/bin/env python
import argparse
import sys
import random
import struct

from scapy.all import sendp, get_if_hwaddr, sniff
from scapy.all import Ether, IPv6, UDP, TCP, Raw


def main():
    if len(sys.argv) < 2:
        print('Usage: recv.py <iface>')
        exit(1)

    iface = sys.argv[1]
    
    def pkt_callback(pkt):
        new_pkt = Ether(src=get_if_hwaddr(iface), dst='00:00:00:e2:0b:00')
        new_pkt = new_pkt / IPv6(src=pkt[IPv6].dst, dst=pkt[IPv6].src) / UDP(dport=pkt[UDP].sport, sport=pkt[UDP].dport) / Raw(load=b"\xff\xff")
        sendp(new_pkt, iface=iface, verbose=False)

    sniff(iface=iface, prn=pkt_callback, filter="udp", store=0)


if __name__ == '__main__':
    main()
