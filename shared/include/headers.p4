#ifndef __HEADERS__
#define __HEADERS__

#include "defines.p4"

#define MAX_SEGMENTS 10

header ethernet_h {
    mac_addr_t dst_addr;
    mac_addr_t src_addr;
    bit<16> ether_type;
}

header ipv6_h {
    bit<4> version;
    bit<8> traffic_class;
    bit<20> flow_label;
    bit<16> payload_len;
    bit<8> next_hdr;
    bit<8> hop_limit;
    ipv6_addr_t src_addr;
    ipv6_addr_t dst_addr;
}

header srv6_h {
    bit<8> next_hdr;
    bit<8> hdr_ext_len;
    bit<8> routing_type;
    bit<8> segment_left;
    bit<8> last_entry;
    bit<8> flags;
    bit<16> tag;
}

header srv6_list_h {
    ipv6_addr_t segment_id;
}

header srv6_ll_tlv_h {
    bit<8> type;
    bit<8> len;
    bit<16> seq_n;
    bit<32> flow_id;
}

header tcp_h {
    bit<16> src_port;
    bit<16> dst_port;
    bit<32> seq_no;
    bit<32> ack_no;
    bit<4>  data_offset;
    bit<3>  res;
    bit<3>  ecn;
    bit<6>  ctrl;
    bit<16> window;
    bit<16> checksum;
    bit<16> urgent_ptr;
}

header udp_h {
    bit<16> src_port;
    bit<16> dst_port;
    bit<16> len;
    bit<16> checksum;
}

header meta_h {
    bit<16> seq_n;
}

header bridge_h {
    bit<16> seq_n;
}

struct metadata {
    bit<8> ip_proto;
    ipv6_addr_t next_srv6_sid;
    l4_lookup_t l4_lookup;
}

struct headers {
    bridge_h bridge;
    ethernet_h ethernet;
    ipv6_h ipv6;
    srv6_h srv6;
    srv6_list_h[MAX_SEGMENTS] srv6_list;
    srv6_ll_tlv_h srv6_ll_tlv;
    ipv6_h ipv6_inner;
    tcp_h tcp;
    udp_h udp;
    meta_h meta;
}

#endif
