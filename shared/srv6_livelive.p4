/* -*- P4_16 -*- */
#include <core.p4>
#include <v1model.p4>

#include "include/defines.p4"
#include "include/headers.p4"
#include "include/parser.p4"
#include "include/checksums.p4"

control IngressPipe(inout headers hdr,
                    inout metadata meta,
                    inout standard_metadata_t standard_metadata) {
    action encapsulate_srv6(bit<128> src_addr) {
        // SRv6 Encapsulation
        hdr.ipv6.payload_len = hdr.ipv6.payload_len + hdr.ipv6_inner.minSizeInBytes() + hdr.srv6.minSizeInBytes();
        hdr.ipv6.next_hdr = PROTO_SRV6;
        hdr.ipv6.src_addr = src_addr;

        hdr.srv6.setValid();
        hdr.srv6.next_hdr = PROTO_IPV6;
        hdr.srv6.routing_type = 0x4;
        hdr.srv6.segment_left = 0;
        hdr.srv6.last_entry = 0;
        hdr.srv6.flags = 0;
        hdr.srv6.tag = 0;

        // Original IPv6 Header
        hdr.ipv6_inner.setValid();
        hdr.ipv6_inner.version = 6;
        hdr.ipv6_inner.traffic_class = hdr.ipv6.traffic_class;
        hdr.ipv6_inner.flow_label = hdr.ipv6.flow_label;
        hdr.ipv6_inner.payload_len = hdr.ipv6.payload_len;
        hdr.ipv6_inner.next_hdr = hdr.ipv6.next_hdr;
        hdr.ipv6_inner.hop_limit = hdr.ipv6.hop_limit;
        hdr.ipv6_inner.src_addr = hdr.ipv6.src_addr;
        hdr.ipv6_inner.dst_addr = hdr.ipv6.dst_addr;
    }

    action live_live_mcast(bit<16> mcast_group, bit<128> src_addr) {
        standard_metadata.mcast_grp = mcast_group;

        encapsulate_srv6(src_addr);
    }

    // TODO: Implement ECMP
    action ipv6_forward(bit<128> src_addr) {
        random(standard_metadata.egress_port, (bit<9>) 2, (bit<9>) 3);

        encapsulate_srv6(src_addr);
    }

    table check_live_live_enabled {
        key = {
            hdr.ipv6.src_addr: lpm;
        }
        actions = {
            ipv6_forward;
            live_live_mcast;
        }
        default_action = ipv6_forward(0);
        size = MAX_NUM_ENTRIES;
    }

    apply {
        if (hdr.ipv6.isValid()) {
            check_live_live_enabled.apply();
        }
    }
}

control EgressPipe(inout headers hdr,
                   inout metadata meta,
                   inout standard_metadata_t standard_metadata) {
    action add_srv6_dest_segment(bit<128> dst_addr) {
        hdr.ipv6.dst_addr = dst_addr;

        hdr.srv6_list.push_front(1);
        hdr.srv6_list[0].setValid();
        hdr.srv6_list[0].segment_id = dst_addr;

        hdr.ipv6.payload_len = hdr.ipv6.payload_len + hdr.srv6_list[0].minSizeInBytes();
    
        hdr.srv6.hdr_ext_len = hdr.srv6.hdr_ext_len + 2;
    }

    action add_srv6_ll_segment(bit<128> ll_func) {
        hdr.srv6_list.push_front(1);
        hdr.srv6_list[0].setValid();
        hdr.srv6_list[0].segment_id = ll_func;

        hdr.srv6.segment_left = hdr.srv6.segment_left + 1;
        hdr.srv6.last_entry = hdr.srv6.last_entry + 1;

        hdr.srv6_ll_tlv.setValid();
        hdr.srv6_ll_tlv.type = 0xff;
        hdr.srv6_ll_tlv.len = 0x06;
        hash(hdr.srv6_ll_tlv.flow_id, HashAlgorithm.crc32, (bit<1>) 0, {hdr.ipv6_inner.src_addr, hdr.ipv6_inner.dst_addr, meta.l4_lookup.src_port, meta.l4_lookup.dst_port, hdr.ipv6_inner.next_hdr}, (bit<32>) 0xffffffff);

        hdr.ipv6.payload_len = hdr.ipv6.payload_len + hdr.srv6_list[0].minSizeInBytes() + hdr.srv6_ll_tlv.minSizeInBytes();

        hdr.srv6.hdr_ext_len = hdr.srv6.hdr_ext_len + 2;
    }

    table srv6_forward {
        key = {
            standard_metadata.egress_port: exact;
        }
        actions = {
            NoAction;
            add_srv6_dest_segment;
        }
        const default_action = NoAction;
        size = MAX_NUM_ENTRIES;
    }

    table srv6_live_live_forward {
        key = {
            standard_metadata.egress_rid: exact;
        }
        actions = {
            NoAction;
            add_srv6_ll_segment;
        }
        const default_action = NoAction;
        size = MAX_NUM_ENTRIES;
    }

    apply { 
        srv6_forward.apply();
        srv6_live_live_forward.apply();
    }
}

V1Switch(
    PktParser(),
    PktVerifyChecksum(),
    IngressPipe(),
    EgressPipe(),
    PktComputeChecksum(),
    PktDeparser()
) main;
