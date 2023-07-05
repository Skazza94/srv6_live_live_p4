#ifndef __PARSER__
#define __PARSER__

#include "defines.p4"

parser PktParser(packet_in packet,
                 out headers hdr,
                 inout metadata meta,
                 inout standard_metadata_t standard_metadata) {
    state start {
        meta.l4_lookup = {0, 0};
        transition parse_ethernet;
    }

    state parse_ethernet {
        packet.extract(hdr.ethernet);
        transition select(hdr.ethernet.ether_type) {
            ETHERTYPE_IPV6: parse_ipv6;
            default: accept;
        }
    }

    state parse_ipv6 {
        packet.extract(hdr.ipv6);
        meta.ip_proto = hdr.ipv6.next_hdr;
        transition select(hdr.ipv6.next_hdr) {
            PROTO_TCP: parse_tcp;
            PROTO_UDP: parse_udp;
            PROTO_SRV6: parse_srv6;
            PROTO_IPV6: parse_ipv6_inner;
            default: accept;
        }
    }

    state parse_srv6 {
        packet.extract(hdr.srv6);
        transition parse_srv6_list;
    }

    state parse_srv6_list {
        packet.extract(hdr.srv6_list.next);
        bool next_segment = (((bit<32>)hdr.srv6.segment_left) - 1) == ((bit<32>) hdr.srv6_list.lastIndex);
        transition select(next_segment) {
            true: mark_current_srv6;
            _: check_last_srv6;
        }
    }

    state mark_current_srv6 {
        meta.next_srv6_sid = hdr.srv6_list.last.segment_id;
        transition check_last_srv6;
    }

    state check_last_srv6 {
        bool last_segment = ((bit<32>) hdr.srv6.last_entry) == ((bit<32>) hdr.srv6_list.lastIndex);
        transition select(last_segment) {
           true: parse_srv6_ll_tlv;
           false: parse_srv6_list;
        }
    }

    state parse_srv6_ll_tlv {
        packet.extract(hdr.srv6_ll_tlv);
        transition parse_srv6_next_hdr;
    }

    state parse_srv6_next_hdr {
        transition select(hdr.srv6.next_hdr) {
            PROTO_TCP: parse_tcp;
            PROTO_UDP: parse_udp;
            PROTO_IPV6: parse_ipv6_inner;
            default: accept;
        }
    }

    state parse_ipv6_inner {
        packet.extract(hdr.ipv6_inner);

        transition select(hdr.ipv6_inner.next_hdr) {
            PROTO_TCP: parse_tcp;
            PROTO_UDP: parse_udp;
            default: accept;
        }
    }

    state parse_tcp {
        packet.extract(hdr.tcp);
        meta.l4_lookup = {hdr.tcp.src_port, hdr.tcp.dst_port};
        transition accept;
    }

    state parse_udp {
        packet.extract(hdr.udp);
        meta.l4_lookup = {hdr.udp.src_port, hdr.udp.dst_port};
        transition accept;
    }
}

control PktDeparser(packet_out packet, in headers hdr) {
    apply {
        packet.emit(hdr.bridge);
        packet.emit(hdr.ethernet);
        packet.emit(hdr.ipv6);
        packet.emit(hdr.srv6);
        packet.emit(hdr.srv6_list);
        packet.emit(hdr.srv6_ll_tlv);
        packet.emit(hdr.ipv6_inner);
        packet.emit(hdr.tcp);
        packet.emit(hdr.udp);
    }
}

#endif