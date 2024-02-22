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
    action srv6_noop(bit<9> egress_port) {
        standard_metadata.egress_spec = egress_port;
    }

    action srv6_seg_ep(bit<9> egress_port) {
        standard_metadata.egress_spec = egress_port;
    }

    table srv6_table {
        key = {
            hdr.ipv6.dst_addr: lpm;
        }
        actions = {
            srv6_noop;
            srv6_seg_ep;
            NoAction;
        }
        default_action = NoAction;
    }

    apply {
        if (hdr.srv6.isValid()) {
            switch (srv6_table.apply().action_run) {
                srv6_seg_ep: {
                    if (hdr.srv6.segment_left > 0) {
                        hdr.ipv6.dst_addr = meta.next_srv6_sid;
                        hdr.srv6.segment_left = hdr.srv6.segment_left - 1;
                    }
                }
            }
        } else {
            mark_to_drop(standard_metadata);
        }
    }
}

control EgressPipe(inout headers hdr,
                   inout metadata meta,
                   inout standard_metadata_t standard_metadata) {
    apply {
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
