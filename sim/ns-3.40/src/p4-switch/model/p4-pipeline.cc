/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2018 Stanford University
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Authors: Stephen Ibanez <sibanez@stanford.edu>
 * Author: Mariano Scazzariello <marianos@kth.se>
 */
#include "p4-pipeline.h"

#include "ns3/ethernet-header.h"
#include "ns3/ipv4-header.h"
#include "ns3/ipv6-header.h"
#include "ns3/log.h"
#include "ns3/tcp-header.h"
#include "ns3/udp-header.h"

#include <bm/bm_runtime/bm_runtime.h>
#include <bm/bm_sim/event_logger.h>
#include <bm/bm_sim/logger.h>
#include <bm/bm_sim/options_parse.h>
#include <bm/bm_sim/parser.h>
#include <bm/bm_sim/tables.h>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <unistd.h>

extern int import_primitives();

namespace ns3
{

namespace
{

struct hash_ex
{
    uint32_t operator()(const char* buf, size_t s) const
    {
        const uint32_t p = 16777619;
        uint32_t hash = 2166136261;

        for (size_t i = 0; i < s; i++)
            hash = (hash ^ buf[i]) * p;

        hash += hash << 13;
        hash ^= hash >> 7;
        hash += hash << 3;
        hash ^= hash >> 17;
        hash += hash << 5;
        return static_cast<uint32_t>(hash);
    }
};

struct bmv2_hash
{
    uint64_t operator()(const char* buf, size_t s) const
    {
        return bm::hash::xxh64(buf, s);
    }
};

} // namespace

// if REGISTER_HASH calls placed in the anonymous namespace, some compiler can
// give an unused variable warning
REGISTER_HASH(hash_ex);
REGISTER_HASH(bmv2_hash);

// initialize static attributes
int P4Pipeline::thrift_port = 9090;
bm::packet_id_t P4Pipeline::packet_id = 0;
uint8_t P4Pipeline::ns2bm_buf[MAX_PKT_SIZE] = {};

P4Pipeline::P4Pipeline(std::string jsonFile)
    : pre(new bm::McSimplePreLAG())
{
    add_component<bm::McSimplePreLAG>(pre);

    // Fields taken from simple_switch
    add_required_field("standard_metadata", "ingress_port");
    add_required_field("standard_metadata", "packet_length");
    add_required_field("standard_metadata", "instance_type");
    add_required_field("standard_metadata", "egress_spec");
    add_required_field("standard_metadata", "egress_port");

    force_arith_header("standard_metadata");
    force_arith_header("queueing_metadata");
    force_arith_header("intrinsic_metadata");

    import_primitives();

    // Initialize the switch
    bm::OptionsParser opt_parser;
    opt_parser.config_file_path = jsonFile;
    opt_parser.debugger_addr =
        std::string("ipc:///tmp/bmv2-") + std::to_string(thrift_port) + std::string("-debug.ipc");
    opt_parser.notifications_addr = std::string("ipc:///tmp/bmv2-") + std::to_string(thrift_port) +
                                    std::string("-notifications.ipc");
    opt_parser.file_logger =
        std::string("/tmp/bmv2-") + std::to_string(thrift_port) + std::string("-pipeline.log");
    opt_parser.thrift_port = thrift_port++;

    int status = init_from_options_parser(opt_parser);
    if (status != 0)
    {
        BMLOG_DEBUG("Failed to initialize the P4 pipeline");
        std::exit(status);
    }
}

void
P4Pipeline::run_cli(std::string commands)
{
    int port = get_runtime_port();
    bm_runtime::start_server(this, port);
    start_and_return();

    std::this_thread::sleep_for(std::chrono::seconds(5));

    // Run the CLI commands to populate table entries
    std::string cmd =
        "run_bmv2_CLI --thrift_port " + std::to_string(port) + " \"" + commands + "\"";
    std::system(cmd.c_str());
}

void
P4Pipeline::start_and_return_()
{
}

int
P4Pipeline::receive_(bm::DevMgr::port_t, const char* buffer, int len)
{
    return 0;
}

std::list<std::pair<uint16_t, Ptr<Packet>>>*
P4Pipeline::process(Ptr<const Packet> ns3_packet, uint32_t ingress_port)
{
    bm::Parser* parser = this->get_parser("parser");
    bm::Deparser* deparser = this->get_deparser("deparser");

    std::list<std::pair<uint16_t, std::unique_ptr<bm::Packet>>>* pkts_to_egress =
        new std::list<std::pair<uint16_t, std::unique_ptr<bm::Packet>>>();
    std::list<std::pair<uint16_t, Ptr<Packet>>>* output_pkts =
        new std::list<std::pair<uint16_t, Ptr<Packet>>>();

    int len = ns3_packet->GetSize();
    auto packet = get_bm_packet(ns3_packet, ingress_port);

    BMELOG(packet_in, *packet);

    bm::PHV* phv = packet->get_phv();
    phv->reset_metadata();
    phv->get_field("standard_metadata.packet_length").set(len);

    bm::Field& f_instance_type = phv->get_field("standard_metadata.instance_type");
    f_instance_type.set(PKT_INSTANCE_TYPE_NORMAL);

    if (phv->has_field("intrinsic_metadata.ingress_global_timestamp"))
    {
        phv->get_field("intrinsic_metadata.ingress_global_timestamp")
            .set(Simulator::Now().GetNanoSeconds());
    }

    phv->get_field("standard_metadata.ingress_port").set(ingress_port);

    const bm::Packet::buffer_state_t packet_in_state = packet->save_buffer_state();
    parser->parse(packet.get());

    if (phv->has_field("standard_metadata.parser_error"))
    {
        phv->get_field("standard_metadata.parser_error").set(packet->get_error_code().get());
    }

    if (phv->has_field("standard_metadata.checksum_error"))
    {
        phv->get_field("standard_metadata.checksum_error")
            .set(packet->get_checksum_error() ? 1 : 0);
    }

    this->process_ingress(packet);

    // Handle Traffic Management, for now only implements multicast
    bm::Field& f_egress_spec_ig = phv->get_field("standard_metadata.egress_spec");
    uint16_t egress_spec_ig = f_egress_spec_ig.get_uint();

    unsigned int mgid = 0u;
    if (phv->has_field("intrinsic_metadata.mcast_grp"))
    {
        bm::Field& f_mgid = phv->get_field("intrinsic_metadata.mcast_grp");
        mgid = f_mgid.get_uint();
    }
    if (mgid != 0)
    {
        BMLOG_DEBUG_PKT(*packet, "Multicast requested for packet");
        auto& f_instance_type = phv->get_field("standard_metadata.instance_type");
        f_instance_type.set(PKT_INSTANCE_TYPE_REPLICATION);
        this->process_multicast(pkts_to_egress, packet.get(), mgid);
    }
    else
    {
        BMLOG_DEBUG_PKT(*packet, "Egress port is {}", egress_spec_ig);
        if (egress_spec_ig == drop_port)
        {
            BMLOG_DEBUG_PKT(*packet, "Dropping packet at ingress");
            packet.release();
        }
        else
        {
            auto& f_instance_type = phv->get_field("standard_metadata.instance_type");
            f_instance_type.set(PKT_INSTANCE_TYPE_NORMAL);

            pkts_to_egress->push_back(std::make_pair(egress_spec_ig, std::move(packet)));
        }
    }

    for (auto& item : *pkts_to_egress)
    {
        std::unique_ptr<bm::Packet> eg_pkt = std::move(item.second);
        this->process_egress(eg_pkt, len, item.first);

        bm::Field& f_egress_spec_eg = phv->get_field("standard_metadata.egress_spec");
        uint16_t egress_spec_eg = f_egress_spec_eg.get_uint();
        if (egress_spec_eg == drop_port)
        {
            BMLOG_DEBUG_PKT(*packet, "Dropping packet at the end of egress");
            eg_pkt.release();
        }
        else
        {
            deparser->deparse(eg_pkt.get());
            output_pkts->push_back(std::make_pair(item.first, get_ns3_packet(std::move(eg_pkt))));
        }
    }

    delete (pkts_to_egress);

    return output_pkts;
}

void
P4Pipeline::process_ingress(std::unique_ptr<bm::Packet>& packet)
{
    // The following is similar to what happens in bmv2 simple_switch ingress thread.
    bm::Pipeline* ingress_mau = this->get_pipeline("ingress");

    BMLOG_DEBUG_PKT(*packet, "Processing packet");

    ingress_mau->apply(packet.get());
    packet->reset_exit();
}

void
P4Pipeline::process_egress(std::unique_ptr<bm::Packet>& packet, int pkt_len, uint16_t egress_port)
{
    // The following is similar to what happens in bmv2 simple_switch egress thread.
    bm::Pipeline* egress_mau = this->get_pipeline("egress");

    bm::PHV* phv = packet->get_phv();

    if (phv->has_field("intrinsic_metadata.egress_global_timestamp"))
    {
        phv->get_field("intrinsic_metadata.egress_global_timestamp")
            .set(Simulator::Now().GetNanoSeconds());
    }

    phv->get_field("standard_metadata.egress_port").set(egress_port);

    bm::Field& f_egress_spec = phv->get_field("standard_metadata.egress_spec");
    f_egress_spec.set(drop_port + 1);

    phv->get_field("standard_metadata.packet_length").set(pkt_len);

    egress_mau->apply(packet.get());
}

void
P4Pipeline::process_multicast(
    std::list<std::pair<uint16_t, std::unique_ptr<bm::Packet>>>* pkts_to_egress,
    bm::Packet* packet,
    unsigned int mgid)
{
    auto* phv = packet->get_phv();
    auto& f_rid = phv->get_field("intrinsic_metadata.egress_rid");
    const auto pre_out = pre->replicate({mgid});
    for (const auto& out : pre_out)
    {
        auto egress_port = out.egress_port;
        BMLOG_DEBUG_PKT(*packet, "Replicating packet on port {}", egress_port);
        f_rid.set(out.rid);
        std::unique_ptr<bm::Packet> packet_copy = packet->clone_with_phv_ptr();
        pkts_to_egress->push_back(std::make_pair(egress_port, std::move(packet_copy)));
    }
}

std::unique_ptr<bm::Packet>
P4Pipeline::get_bm_packet(Ptr<const Packet> ns3_packet, uint32_t ingress_port)
{
    int len = ns3_packet->GetSize();

    if (len > MAX_PKT_SIZE)
    {
        BMLOG_DEBUG("Packet length {} exceeds MAX_PKT_SIZE", len);
        std::exit(1);
    }
    ns3_packet->CopyData(ns2bm_buf, len);
    auto bm_packet = new_packet_ptr(ingress_port,
                                    packet_id++,
                                    len,
                                    bm::PacketBuffer(MAX_PKT_SIZE, (char*)(ns2bm_buf), len));

    return bm_packet;
}

Ptr<Packet>
P4Pipeline::get_ns3_packet(std::unique_ptr<bm::Packet> bm_packet)
{
    uint8_t* bm_buf = (uint8_t*)bm_packet.get()->data();
    size_t len = bm_packet.get()->get_data_size();

    Buffer b;
    b.AddAtStart(len);
    b.Begin().Write(bm_buf, len);

    Buffer::Iterator it = b.Begin();
    EthernetHeader eth;
    eth.Deserialize(it);
    it.Next(eth.GetSerializedSize());
    size_t offset = eth.GetSerializedSize();

    uint16_t ether_type = eth.GetLengthType();

    Ipv4Header ipv4;
    Ipv6Header ipv6;
    TcpHeader tcp;
    UdpHeader udp;
    uint8_t next_hdr = 0;
    if (ether_type == 0x0800)
    {
        ipv4.Deserialize(it);
        it.Next(ipv4.GetSerializedSize());
        offset += ipv4.GetSerializedSize();

        next_hdr = ipv4.GetProtocol();
    }
    else if (ether_type == 0x86dd)
    {
        ipv6.Deserialize(it);
        it.Next(ipv6.GetSerializedSize());
        offset += ipv6.GetSerializedSize();

        next_hdr = ipv6.GetNextHeader();
    }

    if (next_hdr == 6)
    {
        tcp.Deserialize(it);
        it.Next(tcp.GetSerializedSize());
        offset += tcp.GetSerializedSize();
    }
    else if (next_hdr == 17)
    {
        udp.Deserialize(it);
        it.Next(udp.GetSerializedSize());
        offset += udp.GetSerializedSize();
    }

    Ptr<Packet> p = Create<Packet>(bm_buf + offset, len - offset);
    /* Headers are added in reverse order */
    if (next_hdr == 6)
        p->AddHeader(tcp);
    else if (next_hdr == 17)
        p->AddHeader(udp);

    if (ether_type == 0x0800)
        p->AddHeader(ipv4);
    else if (ether_type == 0x86dd)
        p->AddHeader(ipv6);

    p->AddHeader(eth);

    std::cout << "###################################" << std::endl;
    p->Print(std::cout);
    std::cout << std::endl;
    std::cout << "###################################" << std::endl;

    return p;
}
} // namespace ns3
