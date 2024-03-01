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
#include "ns3/flow-monitor-helper.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/p4-switch-module.h"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LiveLiveExample");

std::string
getMacString(uint8_t* mac)
{
    std::string s = "0x";
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (int i = 0; i < 6; i++)
    {
        oss << std::setw(2) << (int)mac[i];
    }
    s += oss.str();

    return s;
}

Mac48Address
convertToMacAddress(Address address)
{
    Mac48Address senderMacAddress = Mac48Address();
    senderMacAddress = senderMacAddress.ConvertFrom(address);
    return senderMacAddress;
}

void
addIpv6ArpEntry(Ptr<Ipv6Interface> interface, Ipv6Address ipv6Address, Mac48Address macAddress)
{
    Ptr<NdiscCache> ndiscCacheSender = interface->GetNdiscCache();
    NdiscCache::Entry* entry = ndiscCacheSender->Lookup(ipv6Address);
    if (!entry)
    {
        std::ostringstream oss;
        oss << "(" << ipv6Address << ", " << macAddress << ")";
        NS_LOG_INFO("ADD an ARP entry for " + oss.str());
        entry = ndiscCacheSender->Add(ipv6Address);
    }
    entry->SetMacAddress(macAddress);
    entry->MarkAutoGenerated();
}

ApplicationContainer
createOnOffTcpApplication(Ipv6Address addressToReach, uint16_t port, Ptr<Node> node)
{
    OnOffHelper onoff("ns3::TcpSocketFactory", Address(Inet6SocketAddress(addressToReach, port)));
    onoff.SetConstantRate(DataRate("500kb/s"));

    ApplicationContainer senderApp = onoff.Install(node);
    return senderApp;
}

ApplicationContainer
createSinkTcpApplication(uint16_t port, Ptr<Node> node)
{
    PacketSinkHelper sink("ns3::TcpSocketFactory",
                          Address(Inet6SocketAddress(Ipv6Address::GetAny(), port)));
    ApplicationContainer receiverApp = sink.Install(node);
    return receiverApp;
}

Ptr<Ipv6Interface>
getIpv6Interface(Ptr<NetDevice> netDevice)
{
    Ptr<Node> node = netDevice->GetNode();
    int32_t interface_index = node->GetObject<Ipv6>()->GetInterfaceForDevice(netDevice);
    return node->GetObject<Ipv6L3Protocol>()->GetInterface(interface_index);
}

void
printRoutes(Ptr<Ipv6StaticRouting> routing)
{
    for (int i = 0; i < routing->GetNRoutes(); i++)
    {
        std::ostringstream oss;
        oss << routing->GetRoute(i);
        std::cout << oss.str() << std::endl;
    }
}

void
addArpEntriesFromInterfaceAddresses(Ptr<Ipv6Interface> nodeInterface,
                                    Ptr<Ipv6Interface> ipv6Interface)
{
    Ipv6StaticRoutingHelper ipv6StaticRouting;
    Ptr<Ipv6StaticRouting> routing = ipv6StaticRouting.GetStaticRouting(
        nodeInterface->GetDevice()->GetNode()->GetObject<Ipv6>());
    for (int i = 1; i < ipv6Interface->GetNAddresses(); i++)
    {
        Ipv6Address address = ipv6Interface->GetAddress(i).GetAddress();
        addIpv6ArpEntry(nodeInterface,
                        address,
                        convertToMacAddress(ipv6Interface->GetDevice()->GetAddress()));

        routing->AddHostRouteTo(address, 1);
    }
    printRoutes(routing);
}

void
addIpv6Addresses(Ptr<Ipv6Interface> ipv6Interface,
                 int addressesNumber,
                 Ipv6AddressHelper ipv6AddressHelper)
{
    for (int i = 0; i < addressesNumber; i++)
    {
        Ipv6Address address = ipv6AddressHelper.NewAddress();

        std::ostringstream oss;
        oss << address;
        NS_LOG_INFO("ADD Ipv6 Address: " + oss.str());

        Ipv6InterfaceAddress interfaceAddress = Ipv6InterfaceAddress(address, Ipv6Prefix(64));
        ipv6Interface->AddAddress(interfaceAddress);
    }
}

std::string
get_path(std::string directory, std::string file)
{
    std::string path = directory;
    if (!directory.empty() && file.back() != '/')
    {
        path += '/';
    }
    path += file;
    return path;
}

int
main(int argc, char* argv[])
{
    LogComponentEnable("LiveLiveExample", LOG_LEVEL_INFO);
    //    LogComponentEnable("FlowMonitor", LOG_LEVEL_DEBUG);
    //    LogComponentEnable("P4SwitchNetDevice", LOG_LEVEL_DEBUG);
    //    LogComponentEnable("TcpSocketBase", LOG_LEVEL_DEBUG);

    int llFlows = 1;
    int concurrentFlowsActive = 1;
    int concurrentFlowsBackup = 1;
    std::string results_path = "src/p4-switch/results";

    CommandLine cmd;
    cmd.AddValue("results-path", "The path where to save results", results_path);
    cmd.AddValue("ll-flows", "The number of concurrent live-live flows to generate", llFlows);
    cmd.AddValue("active-flows",
                 "The number of concurrent flows on the active path",
                 concurrentFlowsActive);
    cmd.AddValue("backup-flows",
                 "The number of concurrent flows on the backup path",
                 concurrentFlowsBackup);
    cmd.Parse(argc, argv);

    NS_LOG_INFO("Results path: " + results_path);
    NS_LOG_INFO("ll-flows: " + std::to_string(llFlows));
    NS_LOG_INFO("active-flows: " + std::to_string(concurrentFlowsActive));
    NS_LOG_INFO("backup-flows: " + std::to_string(concurrentFlowsBackup));

    std::filesystem::remove_all(results_path);
    std::filesystem::create_directories(results_path);

    NS_LOG_INFO("Create nodes.");
    NodeContainer llSenders;
    llSenders.Create(1);

    NodeContainer concurrentSenders;
    concurrentSenders.Create(2);

    NodeContainer llReceivers;
    llReceivers.Create(1);

    NodeContainer concurrentReceivers;
    concurrentReceivers.Create(2);

    NodeContainer llSwitches;
    llSwitches.Create(2);

    NodeContainer forwardSwitches;
    forwardSwitches.Create(2);

    Ptr<Node> e1 = llSwitches.Get(0);
    Ptr<Node> e2 = llSwitches.Get(1);

    Ptr<Node> c1 = forwardSwitches.Get(0);
    Ptr<Node> c2 = forwardSwitches.Get(1);

    Ptr<Node> llReceiver = llReceivers.Get(0);
    Ptr<Node> llSender = llSenders.Get(0);

    Ptr<Node> activeReceiver = concurrentReceivers.Get(0);
    Ptr<Node> backupReceiver = concurrentReceivers.Get(1);

    Ptr<Node> activeSender = concurrentSenders.Get(0);
    Ptr<Node> backupSender = concurrentSenders.Get(1);

    NS_LOG_INFO("Build Topology");
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(5000000));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

    NetDeviceContainer activeSenderInterfaces;
    NetDeviceContainer backupSenderInterfaces;
    NetDeviceContainer llSenderInterfaces;
    NetDeviceContainer activeReceiverInterfaces;
    NetDeviceContainer backupReceiverInterfaces;
    NetDeviceContainer llReceiverInterfaces;
    NetDeviceContainer e1Interfaces;
    NetDeviceContainer e2Interfaces;
    NetDeviceContainer c1Interfaces;
    NetDeviceContainer c2Interfaces;

    NetDeviceContainer link = csma.Install(NodeContainer(llSender, e1));
    llSenderInterfaces.Add(link.Get(0));
    e1Interfaces.Add(link.Get(1));

    link = csma.Install(NodeContainer(activeSender, e1));
    activeSenderInterfaces.Add(link.Get(0));
    e1Interfaces.Add(link.Get(1));

    link = csma.Install(NodeContainer(backupSender, e1));
    backupSenderInterfaces.Add(link.Get(0));
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

    link = csma.Install(NodeContainer(llReceiver, e2));
    llReceiverInterfaces.Add(link.Get(0));
    e2Interfaces.Add(link.Get(1));

    link = csma.Install(NodeContainer(activeReceiver, e2));
    activeReceiverInterfaces.Add(link.Get(0));
    e2Interfaces.Add(link.Get(1));

    link = csma.Install(NodeContainer(backupReceiver, e2));
    backupReceiverInterfaces.Add(link.Get(0));
    e2Interfaces.Add(link.Get(1));

    NS_LOG_INFO("Links Built");
    InternetStackHelper internetV6only;
    internetV6only.SetIpv4StackInstall(false);
    internetV6only.Install(activeSender);
    internetV6only.Install(backupSender);
    internetV6only.Install(activeReceiver);
    internetV6only.Install(backupReceiver);
    internetV6only.Install(llSender);
    internetV6only.Install(llReceiver);

    NS_LOG_INFO("Assign IP Addresses.");

    Ipv6AddressHelper llSenderIpv6Helper;
    llSenderIpv6Helper.SetBase(Ipv6Address("2001::"), Ipv6Prefix(64));
    llSenderIpv6Helper.Assign(llSenderInterfaces);
    Ptr<Ipv6Interface> llSenderIpv6Interface = getIpv6Interface(llSenderInterfaces.Get(0));

    Ipv6AddressHelper llReceiverIpv6Helper;
    llReceiverIpv6Helper.SetBase(Ipv6Address("2002::"), Ipv6Prefix(64));
    llReceiverIpv6Helper.Assign(llReceiverInterfaces);
    Ptr<Ipv6Interface> llReceiverIpv6Interface = getIpv6Interface(llReceiverInterfaces.Get(0));

    Ipv6AddressHelper activeSenderIpv6Helper;
    activeSenderIpv6Helper.SetBase(Ipv6Address("2003::"), Ipv6Prefix(64));
    activeSenderIpv6Helper.Assign(activeSenderInterfaces);
    Ptr<Ipv6Interface> activeSenderIpv6Interface = getIpv6Interface(activeSenderInterfaces.Get(0));

    Ipv6AddressHelper backupSenderIpv6Helper;
    backupSenderIpv6Helper.SetBase(Ipv6Address("2005::"), Ipv6Prefix(64));
    backupSenderIpv6Helper.Assign(backupSenderInterfaces);
    Ptr<Ipv6Interface> backupSenderIpv6Interface = getIpv6Interface(backupSenderInterfaces.Get(0));

    Ipv6AddressHelper activeReceiverIpv6Helper;
    activeReceiverIpv6Helper.SetBase(Ipv6Address("2004::"), Ipv6Prefix(64));
    activeReceiverIpv6Helper.Assign(activeReceiverInterfaces);
    Ptr<Ipv6Interface> activeReceiverIpv6Interface =
        getIpv6Interface(activeReceiverInterfaces.Get(0));

    Ipv6AddressHelper backupReceiverIpv6Helper;
    backupReceiverIpv6Helper.SetBase(Ipv6Address("2006::"), Ipv6Prefix(64));
    backupReceiverIpv6Helper.Assign(backupReceiverInterfaces);
    Ptr<Ipv6Interface> backupReceiverIpv6Interface =
        getIpv6Interface(backupReceiverInterfaces.Get(0));

    NS_LOG_INFO("Configuring llSender address");
    addIpv6Addresses(llSenderIpv6Interface, llFlows, llSenderIpv6Helper);
    addIpv6Addresses(llReceiverIpv6Interface, llFlows, llReceiverIpv6Helper);
    addIpv6Addresses(activeSenderIpv6Interface, 1, activeSenderIpv6Helper);
    addIpv6Addresses(backupSenderIpv6Interface, 1, backupSenderIpv6Helper);
    addIpv6Addresses(activeReceiverIpv6Interface, concurrentFlowsActive, activeReceiverIpv6Helper);
    addIpv6Addresses(backupReceiverIpv6Interface, concurrentFlowsBackup, backupReceiverIpv6Helper);

    addArpEntriesFromInterfaceAddresses(llReceiverIpv6Interface, llSenderIpv6Interface);
    addArpEntriesFromInterfaceAddresses(llSenderIpv6Interface, llReceiverIpv6Interface);
    addArpEntriesFromInterfaceAddresses(activeReceiverIpv6Interface, activeSenderIpv6Interface);
    addArpEntriesFromInterfaceAddresses(backupReceiverIpv6Interface, backupSenderIpv6Interface);
    addArpEntriesFromInterfaceAddresses(activeSenderIpv6Interface, activeReceiverIpv6Interface);
    addArpEntriesFromInterfaceAddresses(backupSenderIpv6Interface, backupReceiverIpv6Interface);

    P4SwitchHelper liveliveHelper;
    liveliveHelper.SetDeviceAttribute(
        "PipelineJson",
        StringValue("/ns3/ns-3.40/src/p4-switch/examples/livelive_build/srv6_livelive.json"));

    uint8_t llMac[6];
    uint8_t activeMac[6];
    uint8_t backupMac[6];
    convertToMacAddress(llSenderIpv6Interface->GetDevice()->GetAddress()).CopyTo(llMac);
    convertToMacAddress(activeSenderIpv6Interface->GetDevice()->GetAddress()).CopyTo(activeMac);
    convertToMacAddress(backupSenderIpv6Interface->GetDevice()->GetAddress()).CopyTo(backupMac);
    std::string e1Commands =
        "mc_mgrp_create 1\nmc_node_create 1 4 5\nmc_node_associate 1 0\n"
        "table_add check_live_live_enabled live_live_mcast 2001::/64 => 1 e1::2\n"
        "table_set_default check_live_live_enabled ipv6_encap_forward_port e1::2 4\n"
        "table_add check_live_live_enabled ipv6_encap_forward_port 2003::/64 => e1::2 4\n"
        "table_add check_live_live_enabled ipv6_encap_forward_port 2005::/64 => e1::2 5\n"
        "table_add srv6_forward add_srv6_dest_segment 4 => e2::2\n"
        "table_add srv6_forward add_srv6_dest_segment 5 => e2::2\n"
        "table_add srv6_live_live_forward add_srv6_ll_segment 1 => e2::55\n"
        "table_add srv6_function srv6_ll_deduplicate 85 => \n"
        "table_add ipv6_forward forward 2001::/64 => 1 " +
        getMacString(llMac) +
        "\n"
        "table_add ipv6_forward forward 2003::/64 => 2 " +
        getMacString(activeMac) +
        "\n"
        "table_add ipv6_forward forward 2005::/64 => 3 " +
        getMacString(backupMac);
    liveliveHelper.SetDeviceAttribute("PipelineCommands", StringValue(e1Commands));
    liveliveHelper.Install(e1, e1Interfaces);

    convertToMacAddress(llReceiverIpv6Interface->GetDevice()->GetAddress()).CopyTo(llMac);
    convertToMacAddress(activeReceiverIpv6Interface->GetDevice()->GetAddress()).CopyTo(activeMac);
    convertToMacAddress(backupReceiverIpv6Interface->GetDevice()->GetAddress()).CopyTo(backupMac);
    std::string e2Commands =
        "mc_mgrp_create 1\nmc_node_create 1 1 2\nmc_node_associate 1 0\n"
        "table_add srv6_function srv6_ll_deduplicate 85 => \n"
        "table_add ipv6_forward forward 2002::/64 => 3 " +
        getMacString(llMac) +
        "\n"
        "table_add ipv6_forward forward 2004::/64 => 4 " +
        getMacString(activeMac) +
        "\n"
        "table_add ipv6_forward forward 2006::/64 => 5 " +
        getMacString(backupMac) +
        "\n"
        "table_add check_live_live_enabled live_live_mcast 2002::/64 => 1 e2::2\n"
        "table_add check_live_live_enabled ipv6_encap_forward_port 2004::/64 => e2::2 1\n"
        "table_add check_live_live_enabled ipv6_encap_forward_port 2006::/64 => e2::2 2\n"
        "table_add srv6_forward add_srv6_dest_segment 1 => e1::2\n"
        "table_add srv6_forward add_srv6_dest_segment 2 => e1::2\n"
        "table_set_default check_live_live_enabled ipv6_encap_forward e2::2 1\n"
        "table_add srv6_live_live_forward add_srv6_ll_segment 1 => e1::55";
    liveliveHelper.SetDeviceAttribute("PipelineCommands", StringValue(e2Commands));
    liveliveHelper.Install(e2, e2Interfaces);

    P4SwitchHelper forwardHelper;
    forwardHelper.SetDeviceAttribute(
        "PipelineJson",
        StringValue("/ns3/ns-3.40/src/p4-switch/examples/forward_build/srv6_forward.json"));
    forwardHelper.SetDeviceAttribute(
        "PipelineCommands",
        StringValue("table_add srv6_table srv6_noop 2002::/64 => 2\n"
                    "table_add srv6_table srv6_seg_ep  e2::2/128 => 2\n"
                    "table_add srv6_table srv6_noop 2001::/64 => 1\n"
                    "table_add srv6_table srv6_seg_ep  e1::2/128 => 1"));
    forwardHelper.Install(c1, c1Interfaces);
    forwardHelper.Install(c2, c2Interfaces);

    NS_LOG_INFO("Create Applications.");
    uint16_t port = 20000;

    NS_LOG_INFO("Create Active Flow Applications.");
    for (int i = 0; i < concurrentFlowsActive; i++)
    {
        ApplicationContainer activeSenderApp =
            createOnOffTcpApplication(activeReceiverIpv6Interface->GetAddress(2 + i).GetAddress(),
                                      port,
                                      activeSender);
        activeSenderApp.Start(Seconds(1.0));
        activeSenderApp.Stop(Seconds(10.0));

        ApplicationContainer activeReceiverApp = createSinkTcpApplication(port + i, activeReceiver);
        activeReceiverApp.Start(Seconds(0.0));
        activeReceiverApp.Stop(Seconds(11.0));
    }

    NS_LOG_INFO("Create Backup Flow Applications.");
    for (int i = 0; i < concurrentFlowsActive; i++)
    {
        ApplicationContainer backupSenderApp =
            createOnOffTcpApplication(backupReceiverIpv6Interface->GetAddress(2 + i).GetAddress(),
                                      port,
                                      backupSender);
        backupSenderApp.Start(Seconds(1.0));
        backupSenderApp.Stop(Seconds(10.0));

        ApplicationContainer backupReceiverApp = createSinkTcpApplication(port + i, backupReceiver);
        backupReceiverApp.Start(Seconds(0.0));
        backupReceiverApp.Stop(Seconds(11.0));
    }

    ApplicationContainer llSenderApp =
        createOnOffTcpApplication(llReceiverIpv6Interface->GetAddress(2).GetAddress(),
                                  port,
                                  llSender);
    llSenderApp.Start(Seconds(1.0));
    llSenderApp.Stop(Seconds(10.0));

    ApplicationContainer llReceiverApp = createSinkTcpApplication(port, llReceiver);
    llReceiverApp.Start(Seconds(0.0));
    llReceiverApp.Stop(Seconds(11.0));

    NS_LOG_INFO("Configure Tracing.");
    AsciiTraceHelper ascii;

    csma.EnableAsciiAll(ascii.CreateFileStream(get_path(results_path, "p4-switch.tr")));
    csma.EnablePcapAll(get_path(results_path, "p4-switch"), true);

    FlowMonitorHelper flowHelper;
    Ptr<FlowMonitor> flowMon = flowHelper.Install(NodeContainer(activeSender,
                                                                activeReceiver,
                                                                backupSender,
                                                                backupReceiver,
                                                                llSender,
                                                                llReceiver));

    NS_LOG_INFO("Run Simulation.");
    Simulator::Stop(Seconds(20));
    Simulator::Run();
    flowMon->CheckForLostPackets();
    flowMon->SerializeToXmlFile(get_path(results_path, "flow_monitor-ll-2-2-1.xml"), true, true);
    Simulator::Destroy();
    NS_LOG_INFO("Done.");
}
