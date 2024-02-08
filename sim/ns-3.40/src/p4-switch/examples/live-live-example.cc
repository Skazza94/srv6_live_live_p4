/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
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
 */

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/p4-switch-module.h"

#include <fstream>
#include <iostream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("P4SwitchExample");

int
main(int argc, char* argv[])
{
    LogComponentEnable("P4SwitchExample", LOG_LEVEL_INFO);
    LogComponentEnable("P4SwitchNetDevice", LOG_LEVEL_DEBUG);

    CommandLine cmd;
    cmd.Parse(argc, argv);

    NS_LOG_INFO("Create nodes.");
    NodeContainer terminals;
    terminals.Create(2);

    NodeContainer switches;
    switches.Create(4);

    NS_LOG_INFO("Build Topology");
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(5000000));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

    NetDeviceContainer terminalDevices;
    NetDeviceContainer e1Interfaces;
    NetDeviceContainer e2Interfaces;
    NetDeviceContainer c1Interfaces;
    NetDeviceContainer c2Interfaces;

    NetDeviceContainer link = csma.Install(NodeContainer(terminals.Get(0), switches.Get(0)));
    terminalDevices.Add(link.Get(0));
    e1Interfaces.Add(link.Get(1));

    link = csma.Install(NodeContainer(switches.Get(0), switches.Get(1)));
    e1Interfaces.Add(link.Get(0));
    c1Interfaces.Add(link.Get(1));

    link = csma.Install(NodeContainer(switches.Get(0), switches.Get(2)));
    e1Interfaces.Add(link.Get(0));
    c2Interfaces.Add(link.Get(1));

    link = csma.Install(NodeContainer(switches.Get(1), switches.Get(3)));
    c1Interfaces.Add(link.Get(0));
    e2Interfaces.Add(link.Get(1));

    link = csma.Install(NodeContainer(switches.Get(2), switches.Get(3)));
    c2Interfaces.Add(link.Get(0));
    e2Interfaces.Add(link.Get(1));

    link = csma.Install(NodeContainer(terminals.Get(1), switches.Get(3)));
    terminalDevices.Add(link.Get(0));
    e2Interfaces.Add(link.Get(1));


    P4SwitchHelper liveliveHelper;
    liveliveHelper.SetDeviceAttribute(
        "PipelineJson",
        StringValue("/ns3/ns-3.40/src/p4-switch/examples/livelive_build/srv6_livelive.json"));
    liveliveHelper.SetDeviceAttribute(
        "PipelineCommands",
        StringValue(
            "mc_mgrp_create 1\nmc_node_create 1 2 3\nmc_node_associate 1 0\n"
            "table_add check_live_live_enabled live_live_mcast 2001::/64 => 1 e1::2\n"
            "table_set_default check_live_live_enabled ipv6_encap_forward e1::2 2\n"
            // "table_add srv6_forward add_srv6_dest_segment 2 => c1::e2\n"
            // "table_add srv6_forward add_srv6_dest_segment 3 => c2::e2\n"
            "table_add srv6_live_live_forward add_srv6_ll_segment 1 => e2::55\n"
            "table_add srv6_function srv6_ll_deduplicate 85 => \n"
            "table_add ipv6_forward forward 2001::/64 => 1 0x0000000ae100"));

    Ptr<Node> e1 = switches.Get(0);
    liveliveHelper.Install(e1, e1Interfaces);

    liveliveHelper.SetDeviceAttribute(
       "PipelineCommands",
       StringValue(
           "mc_mgrp_create 1\nmc_node_create 1 1 2\nmc_node_associate 1 0\n"
           "table_add srv6_function srv6_ll_deduplicate 85 => \n"
           "table_add ipv6_forward forward 2001::2/64 => 3  0x0000000be200\n"
           "table_add check_live_live_enabled live_live_mcast 2001::2/64 => 1 e2::2\n"
           "table_set_default check_live_live_enabled ipv6_encap_forward e2::2 1\n"
           "table_add srv6_live_live_forward add_srv6_ll_segment 1 => e1::55"));

    Ptr<Node> e2 = switches.Get(3);
    liveliveHelper.Install(e2, e2Interfaces);

    P4SwitchHelper forwardHelper;
    forwardHelper.SetDeviceAttribute(
        "PipelineJson",
        StringValue("/ns3/ns-3.40/src/p4-switch/examples/forward_build/srv6_forward.json"));
    forwardHelper.SetDeviceAttribute(
        "PipelineCommands",
        StringValue(
        "table_add srv6_table srv6_noop 2001::1/64 => 2\n"
        "table_add srv6_table srv6_noop 2001::2/64 => 1"));
    Ptr<Node> c1 = switches.Get(1);
    forwardHelper.Install(c1, c1Interfaces);

    Ptr<Node> c2 = switches.Get(2);
    forwardHelper.Install(c2, c2Interfaces);

    InternetStackHelper internetV6only;
    internetV6only.SetIpv4StackInstall(false);
    internetV6only.Install(terminals);

    NS_LOG_INFO("Assign IP Addresses.");
    Ipv6AddressHelper ipv6;
    ipv6.SetBase(Ipv6Address("2001::"), Ipv6Prefix(64));
    ipv6.Assign(terminalDevices);

    Ptr<NetDevice> netDevice0 = terminalDevices.Get(0);
    Ptr<Node> node0 = netDevice0->GetNode();
    int32_t ipv6InterfaceIndex0 = node0->GetObject<Ipv6>()->GetInterfaceForDevice(netDevice0);
    Ptr<Ipv6Interface> ipv6Interface0 =
        node0->GetObject<Ipv6L3Protocol>()->GetInterface(ipv6InterfaceIndex0);
    std::cout << ipv6Interface0 << std::endl;

    Ptr<NetDevice> netDevice1 = terminalDevices.Get(1);
    Ptr<Node> node1 = netDevice1->GetNode();
    int32_t ipv6InterfaceIndex1 = node1->GetObject<Ipv6>()->GetInterfaceForDevice(netDevice1);
    Ptr<Ipv6Interface> ipv6Interface1 =
        node1->GetObject<Ipv6L3Protocol>()->GetInterface(ipv6InterfaceIndex1);
    std::cout << ipv6Interface1 << std::endl;

    Ptr<NdiscCache> ndiscCache = ipv6Interface0->GetNdiscCache();
    NdiscCache::Entry* entry = ndiscCache->Lookup(Ipv6Address("2001::2"));
    if (!entry)
    {
        NS_LOG_INFO("ADD an ARP entry");
        entry = ndiscCache->Add(Ipv6Address("2001::2"));
    }
    entry->SetMacAddress(netDevice1->GetAddress());
    entry->MarkAutoGenerated();

    NS_LOG_INFO("Create Applications.");
    uint16_t port = 9;
    OnOffHelper onoff("ns3::TcpSocketFactory",
                      Address(Inet6SocketAddress(Ipv6Address("2001::2"), port)));
    onoff.SetConstantRate(DataRate("500kb/s"));

    ApplicationContainer app = onoff.Install(terminals.Get(0));
    app.Start(Seconds(1.0));
    app.Stop(Seconds(10.0));

    PacketSinkHelper sink("ns3::TcpSocketFactory",
                          Address(Inet6SocketAddress(Ipv6Address::GetAny(), port)));
    app = sink.Install(terminals.Get(1));
    app.Start(Seconds(0.0));

    NS_LOG_INFO("Configure Tracing.");
    AsciiTraceHelper ascii;
    csma.EnableAsciiAll(ascii.CreateFileStream("p4-switch.tr"));
    csma.EnablePcapAll("p4-switch", true);

    NS_LOG_INFO("Run Simulation.");
    Simulator::Run();
    Simulator::Destroy();
    NS_LOG_INFO("Done.");
}
