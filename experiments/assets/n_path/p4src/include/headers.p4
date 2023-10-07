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

header tcp_option_end_h {
    bit<8> kind;
}

header tcp_option_nop_h {
    bit<8> kind;
}

header tcp_option_ss_h {
    bit<8>  kind;
    bit<32> maxSegmentSize;
}

header tcp_option_s_h {
    bit<8>  kind;
    bit<24> scale;
}

header tcp_option_sack_h {
    bit<8>         kind;
    bit<8>         length;
    varbit<256>    sack;
}

header_union tcp_option_h {
    tcp_option_end_h  end;
    tcp_option_nop_h  nop;
    tcp_option_ss_h   ss;
    tcp_option_s_h    s;
    tcp_option_sack_h sack;
}

// Defines a stack of 10 tcp options
typedef tcp_option_h[10] tcp_option_stack;

header tcp_option_padding_h {
    varbit<256> padding;
}

error {
    TcpDataOffsetTooSmall,
    TcpOptionTooLongForHeader,
    TcpBadSackOptionLength
}

struct tcp_option_sack_top
{
    bit<8> kind;
    bit<8> length;
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
    tcp_option_stack tcp_options_vec;
    tcp_option_padding_h tcp_options_padding;
    udp_h udp;
    meta_h meta;
}

#endif
