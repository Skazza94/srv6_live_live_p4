#ifndef __PARSER__
#define __PARSER__

#include "defines.p4"

parser tcp_option_parser(packet_in b,
                         in bit<4> tcp_hdr_data_offset,
                         out tcp_option_stack vec,
                         out tcp_option_padding_h padding)
{
    bit<7> tcp_hdr_bytes_left;
    
    state start {
        // RFC 793 - the Data Offset field is the length of the TCP
        // header in units of 32-bit words.  It must be at least 5 for
        // the minimum length TCP header, and since it is 4 bits in
        // size, can be at most 15, for a maximum TCP header length of
        // 15*4 = 60 bytes.
        verify(tcp_hdr_data_offset >= 5, error.TcpDataOffsetTooSmall);
        tcp_hdr_bytes_left = 4 * (bit<7>) (tcp_hdr_data_offset - 5);
        // always true here: 0 <= tcp_hdr_bytes_left <= 40
        transition next_option;
    }
    state next_option {
        transition select(tcp_hdr_bytes_left) {
            0 : accept;  // no TCP header bytes left
            default : next_option_part2;
        }
    }
    state next_option_part2 {
        // precondition: tcp_hdr_bytes_left >= 1
        transition select(b.lookahead<bit<8>>()) {
            0: parse_tcp_option_end;
            1: parse_tcp_option_nop;
            2: parse_tcp_option_ss;
            3: parse_tcp_option_s;
            5: parse_tcp_option_sack;
        }
    }
    state parse_tcp_option_end {
        b.extract(vec.next.end);
        // TBD: This code is an example demonstrating why it would be
        // useful to have sizeof(vec.next.end) instead of having to
        // put in a hard-coded length for each TCP option.
        tcp_hdr_bytes_left = tcp_hdr_bytes_left - 1;
        transition consume_remaining_tcp_hdr_and_accept;
    }
    state consume_remaining_tcp_hdr_and_accept {
        // A more picky sub-parser implementation would verify that
        // all of the remaining bytes are 0, as specified in RFC 793,
        // setting an error and rejecting if not.  This one skips past
        // the rest of the TCP header without checking this.

        // tcp_hdr_bytes_left might be as large as 40, so multiplying
        // it by 8 it may be up to 320, which requires 9 bits to avoid
        // losing any information.
        b.extract(padding, (bit<32>) (8 * (bit<9>) tcp_hdr_bytes_left));
        transition accept;
    }
    state parse_tcp_option_nop {
        b.extract(vec.next.nop);
        tcp_hdr_bytes_left = tcp_hdr_bytes_left - 1;
        transition next_option;
    }
    state parse_tcp_option_ss {
        verify(tcp_hdr_bytes_left >= 5, error.TcpOptionTooLongForHeader);
        tcp_hdr_bytes_left = tcp_hdr_bytes_left - 5;
        b.extract(vec.next.ss);
        transition next_option;
    }
    state parse_tcp_option_s {
        verify(tcp_hdr_bytes_left >= 4, error.TcpOptionTooLongForHeader);
        tcp_hdr_bytes_left = tcp_hdr_bytes_left - 4;
        b.extract(vec.next.s);
        transition next_option;
    }
    state parse_tcp_option_sack {
        bit<8> n_sack_bytes = b.lookahead<tcp_option_sack_top>().length;
        // I do not have global knowledge of all TCP SACK
        // implementations, but from reading the RFC, it appears that
        // the only SACK option lengths that are legal are 2+8*n for
        // n=1, 2, 3, or 4, so set an error if anything else is seen.
        verify(n_sack_bytes == 10 || n_sack_bytes == 18 ||
               n_sack_bytes == 26 || n_sack_bytes == 34,
               error.TcpBadSackOptionLength);
        verify(tcp_hdr_bytes_left >= (bit<7>) n_sack_bytes,
               error.TcpOptionTooLongForHeader);
        tcp_hdr_bytes_left = tcp_hdr_bytes_left - (bit<7>) n_sack_bytes;
        b.extract(vec.next.sack, (bit<32>) (8 * n_sack_bytes - 16));
        transition next_option;
    }
}

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
        tcp_option_parser.apply(packet, hdr.tcp.data_offset,
                                hdr.tcp_options_vec, hdr.tcp_options_padding);
        transition accept;
    }

    state parse_udp {
        packet.extract(hdr.udp);
        meta.l4_lookup = {hdr.udp.src_port, hdr.udp.dst_port};
        transition parse_meta;
    }

    state parse_meta {
        packet.extract(hdr.meta);
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
        packet.emit(hdr.tcp_options_vec);
        packet.emit(hdr.tcp_options_padding);
        packet.emit(hdr.udp);
        packet.emit(hdr.meta);
    }
}

#endif