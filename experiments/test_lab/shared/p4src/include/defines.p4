#ifndef __DEFINES__
#define __DEFINES__

typedef bit<48>  mac_addr_t;
typedef bit<128> ipv6_addr_t;

const bit<16> ETHERTYPE_IPV6 = 0x86dd;

const bit<8> PROTO_TCP = 6;
const bit<8> PROTO_UDP = 17;
const bit<8> PROTO_SRV6 = 43;
const bit<8> PROTO_IPV6 = 41;

#define MAX_NUM_ENTRIES 2048

/* Struct to store L4 ports */
struct l4_lookup_t {
    bit<16> src_port;
    bit<16> dst_port;
}

#endif