/* -*- P4_16 -*- */
#include <core.p4>
#include <v1model.p4>

#include "include/defines.p4"
#include "include/headers.p4"
#include "include/parser.p4"
#include "include/checksums.p4"

#define MAX_NUM_FLOWS 1
#define WINDOW_SIZE 64

control IngressPipe(inout headers hdr,
                    inout metadata meta,
                    inout standard_metadata_t standard_metadata) {
    register<bit<16>>(MAX_NUM_FLOWS) seq_n;

    action encapsulate_srv6(bit<128> src_addr) {
        bit<16> original_len = hdr.ipv6.payload_len;
        bit<8> original_next_hdr = hdr.ipv6.next_hdr;
        bit<128> original_src_addr = hdr.ipv6.src_addr;

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
        hdr.ipv6_inner.payload_len = original_len;
        hdr.ipv6_inner.next_hdr = original_next_hdr;
        hdr.ipv6_inner.hop_limit = hdr.ipv6.hop_limit;
        hdr.ipv6_inner.src_addr = original_src_addr;
        hdr.ipv6_inner.dst_addr = hdr.ipv6.dst_addr;
    }

    action live_live_mcast(bit<16> mcast_group, bit<128> src_addr) {
        standard_metadata.mcast_grp = mcast_group;

        encapsulate_srv6(src_addr);
        hdr.srv6.tag = 1;
        // Tag the packet with a sequence number
        hdr.bridge.setValid();

        bit<32> flow_id = 0;
        hash(flow_id, HashAlgorithm.crc32, (bit<1>) 0, {hdr.ipv6_inner.src_addr, hdr.ipv6_inner.dst_addr, meta.l4_lookup.src_port, meta.l4_lookup.dst_port, hdr.ipv6_inner.next_hdr}, (bit<32>) MAX_NUM_FLOWS);
        bit<16> curr_seq_n = 0;
        seq_n.read(curr_seq_n, flow_id);
        hdr.bridge.seq_n = curr_seq_n + 1;
        seq_n.write(flow_id, hdr.bridge.seq_n);
    }

    action ipv6_encap_forward(bit<128> src_addr, bit<9> port) {
        standard_metadata.egress_spec = port;
        
        encapsulate_srv6(src_addr);
    }

    table check_live_live_enabled {
        key = {
            hdr.ipv6.src_addr: lpm;
        }
        actions = {
            ipv6_encap_forward;
            live_live_mcast;
        }
        default_action = ipv6_encap_forward(0, 1);
        size = MAX_NUM_ENTRIES;
    }

    register<bit<WINDOW_SIZE>>(MAX_NUM_FLOWS) flow_to_bitmap;
    register<bit<16>>(MAX_NUM_FLOWS) flow_window_start;

    action srv6_ll_deduplicate() {
    }

    bit<64> srv6_func_id = 0;
    table srv6_function {
        key = {
            srv6_func_id: exact;
        }
        actions = {
            NoAction;
            srv6_ll_deduplicate;
        }
        default_action = NoAction;
        size = MAX_NUM_ENTRIES;
    }

    action forward(bit<9> egress_port, bit<48> mac_dst) {
        standard_metadata.egress_spec = egress_port;
        hdr.ethernet.dst_addr = mac_dst;
    }

    table ipv6_forward {
        key = {
            hdr.ipv6_inner.dst_addr: lpm;
        }
        actions = {
            NoAction;
            forward;
        }
        default_action = NoAction;
        size = MAX_NUM_ENTRIES;
    }

    apply {
        if (hdr.ipv6.isValid()) {
            if (!hdr.srv6.isValid()) {
                check_live_live_enabled.apply();
            } else if (hdr.srv6.segment_left == 0) {
                srv6_func_id = hdr.srv6_list[0].segment_id[63:0];
                if(srv6_function.apply().hit) {
                    bit<16> start_value;
                    flow_window_start.read(start_value, hdr.srv6_ll_tlv.flow_id);
                    bit<WINDOW_SIZE> curr_bitmap;
                    flow_to_bitmap.read(curr_bitmap, hdr.srv6_ll_tlv.flow_id);

                    if (hdr.srv6_ll_tlv.seq_n > start_value + (WINDOW_SIZE - 1)) {
                        log_msg("hdr.srv6_ll_tlv.seq_n {}", {hdr.srv6_ll_tlv.seq_n});
                        log_msg("start_value: {}, WINDOW_SIZE: {}, start_value + WINDOW_SIZE {}", {start_value, (bit<8>) WINDOW_SIZE, start_value + WINDOW_SIZE});

                        bit<8> shift_idx = (bit<8>) (hdr.srv6_ll_tlv.seq_n - (WINDOW_SIZE - 1) - start_value);
                        if (shift_idx > 63) {
                            curr_bitmap = 0x0;
                        } else {
                            curr_bitmap = curr_bitmap >> shift_idx;
                        }
                        start_value = hdr.srv6_ll_tlv.seq_n - (WINDOW_SIZE - 1);
                        flow_window_start.write(hdr.srv6_ll_tlv.flow_id, start_value);
                    }
                    
                    bit<8> window_idx = (bit<8>) (hdr.srv6_ll_tlv.seq_n - start_value);
                    bit<64> idx_bitmask = (bit<64>) 1 << window_idx;
                    bit<64> relevant_bit = curr_bitmap & idx_bitmask;
                    bit<1> already_received = (bit<1>) (relevant_bit >> window_idx);

                    if (already_received == 1) {
                        log_msg("Dropping packet of flow {} with index {}", {hdr.srv6_ll_tlv.flow_id, hdr.srv6_ll_tlv.seq_n});
                        mark_to_drop(standard_metadata);
                    } else {
                        log_msg("Forwarding packet of flow {} with index {}", {hdr.srv6_ll_tlv.flow_id, hdr.srv6_ll_tlv.seq_n});

                        curr_bitmap = curr_bitmap | idx_bitmask;
                        flow_to_bitmap.write(hdr.srv6_ll_tlv.flow_id, curr_bitmap);

                        hdr.ipv6.setInvalid();
                        hdr.srv6.setInvalid();
                        hdr.srv6_list[0].setInvalid();
                        hdr.srv6_list[1].setInvalid();
                        hdr.srv6_list[2].setInvalid();
                        hdr.srv6_list[3].setInvalid();
                        hdr.srv6_list[4].setInvalid();
                        hdr.srv6_list[5].setInvalid();
                        hdr.srv6_list[6].setInvalid();
                        hdr.srv6_list[7].setInvalid();
                        hdr.srv6_list[8].setInvalid();
                        hdr.srv6_list[9].setInvalid();
                        hdr.srv6_ll_tlv.setInvalid();

                        ipv6_forward.apply();
                    }
                } else {
                    hdr.ipv6.setInvalid();
                    hdr.srv6.setInvalid();
                    hdr.srv6_list[0].setInvalid();
                    hdr.srv6_list[1].setInvalid();
                    hdr.srv6_list[2].setInvalid();
                    hdr.srv6_list[3].setInvalid();
                    hdr.srv6_list[4].setInvalid();
                    hdr.srv6_list[5].setInvalid();
                    hdr.srv6_list[6].setInvalid();
                    hdr.srv6_list[7].setInvalid();
                    hdr.srv6_list[8].setInvalid();
                    hdr.srv6_list[9].setInvalid();
                    hdr.srv6_ll_tlv.setInvalid();

                    ipv6_forward.apply();
                }
            }
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
        hash(hdr.srv6_ll_tlv.flow_id, HashAlgorithm.crc32, (bit<1>) 0, {hdr.ipv6_inner.src_addr, hdr.ipv6_inner.dst_addr, meta.l4_lookup.src_port, meta.l4_lookup.dst_port, hdr.ipv6_inner.next_hdr}, (bit<32>) MAX_NUM_FLOWS);
        hdr.srv6_ll_tlv.seq_n = hdr.bridge.seq_n;
        hdr.meta.seq_n = hdr.bridge.seq_n;
        hdr.bridge.setInvalid();

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
