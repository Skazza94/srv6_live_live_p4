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
#include "ns3/flow-monitor-module.h"
#include "ns3/flow-monitor-helper.h"

#include <fstream>
#include <iostream>
#include <iomanip>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LiveLiveExample");

std::string getMacString(uint8_t *mac) {
    std::string s = "0x";
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (int i = 0; i < 6; i++) {
        oss << std::setw(2) << (int) mac[i];
    }
    s += oss.str();

    return s;
}

int
main(int argc, char *argv[]) {
    LogComponentEnable("LiveLiveExample", LOG_LEVEL_INFO);
    LogComponentEnable("FlowMonitor", LOG_LEVEL_DEBUG);
//    LogComponentEnable("P4SwitchNetDevice", LOG_LEVEL_DEBUG);
//    LogComponentEnable("TcpSocketBase", LOG_LEVEL_DEBUG);

    CommandLine cmd;
    cmd.Parse(argc, argv);

    NS_LOG_INFO("Create nodes.");
    NodeContainer sender;
    sender.Create(1);

    NodeContainer receiver;
    receiver.Create(1);

    NodeContainer switches;
    switches.Create(4);

    NS_LOG_INFO("Build Topology");
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(5000000));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

    Ptr<Node> e1 = switches.Get(0);
    Ptr<Node> c1 = switches.Get(1);
    Ptr<Node> c2 = switches.Get(2);
    Ptr<Node> e2 = switches.Get(3);

    NetDeviceContainer senderInterfaces;
    NetDeviceContainer receiverInterfaces;
    NetDeviceContainer e1Interfaces;
    NetDeviceContainer e2Interfaces;
    NetDeviceContainer c1Interfaces;
    NetDeviceContainer c2Interfaces;

    NetDeviceContainer link = csma.Install(NodeContainer(sender.Get(0), e1));
    senderInterfaces.Add(link.Get(0));
    e1Interfaces.Add(link.Get(1));

    link = csma.Install(NodeContainer(e1, c1));
    e1Interfaces.Add(link.Get(0));
    c1Interfaces.Add(link.Get(1));

    link = csma.Install(NodeContainer(e1, c2));
    e1Interfaces.Add(link.Get(0));
    c2Interfaces.Add(link.Get(1));

    link = csma.Install(NodeContainer(c1, e2));
    c1Interfaces.Add(link.Get(0));
    e2Interfaces.Add(link.Get(1));

    link = csma.Install(NodeContainer(c2, e2));
    c2Interfaces.Add(link.Get(0));
    e2Interfaces.Add(link.Get(1));

    link = csma.Install(NodeContainer(receiver.Get(0), e2));
    receiverInterfaces.Add(link.Get(0));
    e2Interfaces.Add(link.Get(1));

    P4SwitchHelper forwardHelper;
    forwardHelper.SetDeviceAttribute(
            "PipelineJson",
            StringValue("/ns3/ns-3.40/src/p4-switch/examples/forward_build/srv6_forward.json"));
    forwardHelper.SetDeviceAttribute(
            "PipelineCommands",
            StringValue(
                    "table_add srv6_table srv6_noop 2002::/64 => 2\n"
                    "table_add srv6_table srv6_noop 2001::/64 => 1"));
    forwardHelper.Install(c1, c1Interfaces);
    forwardHelper.Install(c2, c2Interfaces);

    InternetStackHelper internetV6only;
    internetV6only.SetIpv4StackInstall(false);
    internetV6only.Install(sender);
    internetV6only.Install(receiver);

    NS_LOG_INFO("Assign IP Addresses.");
    Ipv6AddressHelper ipv6;
    ipv6.SetBase(Ipv6Address("2001::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer senderIpv6InterfaceContainer = ipv6.Assign(senderInterfaces);

    ipv6.SetBase(Ipv6Address("2002::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer receiverIpv6InterfaceContainer = ipv6.Assign(receiverInterfaces);

    Ptr<NetDevice> senderDevice = senderInterfaces.Get(0);
    Ptr<Node> node0 = senderDevice->GetNode();
    int32_t ipv6InterfaceIndex0 = node0->GetObject<Ipv6>()->GetInterfaceForDevice(senderDevice);
    Ptr<Ipv6Interface> senderIpv6Interface =
            node0->GetObject<Ipv6L3Protocol>()->GetInterface(ipv6InterfaceIndex0);

    Ptr<NetDevice> receiverDevice = receiverInterfaces.Get(0);
    Ptr<Node> node1 = receiverDevice->GetNode();
    int32_t ipv6InterfaceIndex1 = node1->GetObject<Ipv6>()->GetInterfaceForDevice(receiverDevice);
    Ptr<Ipv6Interface> receiverIpv6Interface =
            node1->GetObject<Ipv6L3Protocol>()->GetInterface(ipv6InterfaceIndex1);

    Ipv6Address senderIpv6Address = senderIpv6Interface->GetAddress(1).GetAddress();
    Mac48Address senderMacAddress = Mac48Address();
    senderMacAddress = senderMacAddress.ConvertFrom(senderDevice->GetAddress());

    Ipv6Address receiverIpv6Address = receiverIpv6Interface->GetAddress(1).GetAddress();
    Mac48Address receiverMacAddress = Mac48Address();
    receiverMacAddress = receiverMacAddress.ConvertFrom(receiverDevice->GetAddress());

    NS_LOG_INFO("############# Devices Configuration ##############");
    NS_LOG_INFO("Sender Mac Address: " << senderMacAddress);
    NS_LOG_INFO("Sender Ipv6 Address: " << senderIpv6Address);
    NS_LOG_INFO("Receiver Mac Address: " << receiverMacAddress);
    NS_LOG_INFO("Receiver Ipv6 Address: " << receiverIpv6Address);


    Ptr<NdiscCache> ndiscCacheSender = senderIpv6Interface->GetNdiscCache();
    NdiscCache::Entry *entry = ndiscCacheSender->Lookup(receiverIpv6Address);
    if (!entry) {
        NS_LOG_INFO("ADD an ARP entry");
        entry = ndiscCacheSender->Add(receiverIpv6Address);
    }
    entry->SetMacAddress(receiverMacAddress);
    entry->MarkAutoGenerated();

    Ptr<NdiscCache> ndiscCacheReceiver = receiverIpv6Interface->GetNdiscCache();
    entry = ndiscCacheReceiver->Lookup(senderIpv6Address);
    if (!entry) {
        NS_LOG_INFO("ADD an ARP entry");
        entry = ndiscCacheReceiver->Add(senderIpv6Address);
    }
    entry->SetMacAddress(senderMacAddress);
    entry->MarkAutoGenerated();

    Ipv6StaticRoutingHelper ipv6StaticRouting;
    Ptr<Ipv6StaticRouting> routing = ipv6StaticRouting.GetStaticRouting(sender.Get(0)->GetObject<Ipv6>());
    routing->AddHostRouteTo(receiverIpv6Address,
                            senderIpv6InterfaceContainer.GetInterfaceIndex(0));

    routing = ipv6StaticRouting.GetStaticRouting(receiver.Get(0)->GetObject<Ipv6>());
    routing->AddHostRouteTo(senderIpv6Interface->GetAddress(1).GetAddress(),
                            receiverIpv6InterfaceContainer.GetInterfaceIndex(0));

    P4SwitchHelper liveliveHelper;
    liveliveHelper.SetDeviceAttribute(
            "PipelineJson",
            StringValue("/ns3/ns-3.40/src/p4-switch/examples/livelive_build/srv6_livelive.json"));

    uint8_t mac[6];
    senderMacAddress.CopyTo(mac);
    std::string e1Commands = "mc_mgrp_create 1\nmc_node_create 1 2 3\nmc_node_associate 1 0\n"
                             "table_add check_live_live_enabled live_live_mcast 2001::/64 => 1 e1::2\n"
                             "table_set_default check_live_live_enabled ipv6_encap_forward e1::2 2\n"
                             "table_add srv6_live_live_forward add_srv6_ll_segment 1 => e2::55\n"
                             "table_add srv6_function srv6_ll_deduplicate 85 => \n"
                             "table_add ipv6_forward forward 2001::/64 => 1 " + getMacString(mac);
    liveliveHelper.SetDeviceAttribute(
            "PipelineCommands",
            StringValue(e1Commands));
    liveliveHelper.Install(e1, e1Interfaces);

    receiverMacAddress.CopyTo(mac);
    std::string e2Commands = "mc_mgrp_create 1\nmc_node_create 1 1 2\nmc_node_associate 1 0\n"
                             "table_add srv6_function srv6_ll_deduplicate 85 => \n"
                             "table_add ipv6_forward forward 2002::/64 => 3 " + getMacString(mac) + "\n"
                             "table_add check_live_live_enabled live_live_mcast 2002::/64 => 1 e2::2\n"
                             "table_set_default check_live_live_enabled ipv6_encap_forward e2::2 1\n"
                             "table_add srv6_live_live_forward add_srv6_ll_segment 1 => e1::55";
    liveliveHelper.SetDeviceAttribute(
            "PipelineCommands",
            StringValue(e2Commands));
    liveliveHelper.Install(e2, e2Interfaces);

    NS_LOG_INFO("Create Applications.");
    uint16_t port = 20000;
    OnOffHelper onoff("ns3::TcpSocketFactory",
                      Address(Inet6SocketAddress(receiverIpv6Address, port)));
    onoff.SetConstantRate(DataRate("500kb/s"));

    ApplicationContainer senderApp = onoff.Install(sender.Get(0));
    senderApp.Start(Seconds(1.0));
    senderApp.Stop(Seconds(10.0));

    PacketSinkHelper sink("ns3::TcpSocketFactory",
                          Address(Inet6SocketAddress(Ipv6Address::GetAny(), port)));
    ApplicationContainer receiverApp = sink.Install(receiver.Get(0));
    receiverApp.Start(Seconds(0.0));
    receiverApp.Stop(Seconds(11.0));

    NS_LOG_INFO("Configure Tracing.");
    AsciiTraceHelper ascii;
    csma.EnableAsciiAll(ascii.CreateFileStream("p4-switch.tr"));
    csma.EnablePcapAll("p4-switch", true);

    FlowMonitorHelper flowHelper;
    Ptr<FlowMonitor> flowMon = flowHelper.Install(NodeContainer(sender.Get(0), receiver.Get(0)));

    NS_LOG_INFO("Run Simulation.");
    Simulator::Stop(Seconds(20));
    Simulator::Run();
    flowMon->CheckForLostPackets();
    flowMon->SerializeToXmlFile("flow_monitor-ll-2-2-1.xml", true, true);
    Simulator::Destroy();
    NS_LOG_INFO("Done.");
}
