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
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LiveLiveExample");

bool verbose = false;

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

static void
CwndChange(Ptr<OutputStreamWrapper> stream, uint32_t oldCwnd, uint32_t newCwnd)
{
    /*NS_LOG_UNCOND(Simulator::Now().GetSeconds() << "\t" << newCwnd);
    *stream->GetStream() << Simulator::Now().GetSeconds() << "\t" << oldCwnd << "\t" << newCwnd
                         << std::endl;
*/}

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
            if (verbose)
            {
                NS_LOG_INFO("Add ARP Entry for " << "(" << ipv6Address << ", " << macAddress
                                                 << ")");
            }
            entry = ndiscCacheSender->Add(ipv6Address);
        }
        entry->SetMacAddress(macAddress);
        entry->MarkAutoGenerated();
    }

    ApplicationContainer
    createOnOffTcpApplication(Ipv6Address addressToReach,
                              uint16_t port,
                              Ptr<Node> node,
                              std::string data_rate,
                              uint32_t max_bytes)
    {
        OnOffHelper onoff("ns3::TcpSocketFactory",
                          Address(Inet6SocketAddress(addressToReach, port)));
        onoff.SetAttribute("DataRate", StringValue(data_rate));
        onoff.SetAttribute("MaxBytes", UintegerValue(max_bytes));
        onoff.SetAttribute("PacketSize", UintegerValue(1500));

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
        for (uint32_t i = 0; i < routing->GetNRoutes(); i++)
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
        for (uint32_t i = 1; i < ipv6Interface->GetNAddresses(); i++)
        {
            Ipv6Address address = ipv6Interface->GetAddress(i).GetAddress();
            addIpv6ArpEntry(nodeInterface,
                            address,
                            convertToMacAddress(ipv6Interface->GetDevice()->GetAddress()));

            routing->AddHostRouteTo(address, 1);
        }

        if (verbose)
        {
            printRoutes(routing);
        }
    }

    void
    addIpv6Address(Ptr<Ipv6Interface> ipv6Interface, Ipv6AddressHelper* ipv6AddressHelper)
    {
        Ipv6Address address = ipv6AddressHelper->NewAddress();
        if (verbose)
        {
            NS_LOG_INFO("Add IPv6 Address: " << address);
        }
        Ipv6InterfaceAddress interfaceAddress = Ipv6InterfaceAddress(address, Ipv6Prefix(64));
        ipv6Interface->AddAddress(interfaceAddress);
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

    static std::map<uint32_t, bool> firstCwnd;   //!< First congestion window.
    static std::map<uint32_t, bool> firstSshThr; //!< First SlowStart threshold.
    static std::map<uint32_t, bool> firstRtt;    //!< First RTT.
    static std::map<uint32_t, bool> firstRto;    //!< First RTO.
    static std::map<uint32_t, Ptr<OutputStreamWrapper>>
        cWndStream;                                //!< Congstion window output stream.
    static std::map<uint32_t, uint32_t> cWndValue; //!< congestion window value.

    static uint32_t
    GetNodeIdFromContext(std::string context)
    {
        const std::size_t n1 = context.find_first_of('/', 1);
        const std::size_t n2 = context.find_first_of('/', n1 + 1);
        return std::stoul(context.substr(n1 + 1, n2 - n1 - 1));
    }

    static void
    CwndTracer(std::string context, uint32_t oldval, uint32_t newval)
    {
        uint32_t nodeId = GetNodeIdFromContext(context);

        if (firstCwnd[nodeId])
        {
            *cWndStream[nodeId]->GetStream() << "0.0 " << oldval << std::endl;
            firstCwnd[nodeId] = false;
        }
        *cWndStream[nodeId]->GetStream()
            << Simulator::Now().GetSeconds() << " " << newval << std::endl;
        cWndValue[nodeId] = newval;
    }

    static void
    TraceCwnd(std::string cwnd_tr_file_name, uint32_t nodeId)
    {
        AsciiTraceHelper ascii;
        auto it = cWndStream.find(nodeId);
        if (it == cWndStream.end())
            cWndStream[nodeId] = ascii.CreateFileStream(cwnd_tr_file_name);

        Config::Connect("/NodeList/" + std::to_string(nodeId) +
                            "/$ns3::TcpL4Protocol/SocketList/0/CongestionWindow",
                        MakeCallback(&CwndTracer));
    }

    int
    main(int argc, char* argv[])
    {
        uint32_t llFlows = 1;
        uint32_t activeFlows = 1;
        uint32_t backupFlows = 1;
        std::string resultsPath = "examples/srv6-live-live/results/";
        std::string defaultBandwidth = "50Kbps";
        std::string llRate = "50Kbps";
        std::string activeBandwidth = "50Kbps";
        uint32_t activeDelay = 0;
        std::string activeRate = "50Kbps";
        std::string backupBandwidth = "50Kbps";
        uint32_t backupDelay = 0;
        std::string backupRate = "50Kbps";
        std::string congestionControl = "ns3::TcpLinuxReno";
        bool dumpTraffic = false;

        CommandLine cmd;
        cmd.AddValue("results-path", "The path where to save results", resultsPath);
        cmd.AddValue("ll-flows", "The number of concurrent live-live flows to generate", llFlows);
        cmd.AddValue("active-flows",
                     "The number of concurrent flows on the active path",
                     activeFlows);
        cmd.AddValue("backup-flows",
                     "The number of concurrent flows on the backup path",
                     backupFlows);
        cmd.AddValue("default-bw",
                     "The bandwidth to set on all the sender/receiver links",
                     defaultBandwidth);
        cmd.AddValue("ll-rate", "The TCP rate to set to the live-live flows", llRate);
        cmd.AddValue("active-bw", "The bandwidth to set on the active path", activeBandwidth);
        cmd.AddValue("active-delay", "The delay to set on the active path", activeDelay);
        cmd.AddValue("active-rate", "The TCP rate to set to the active flows", activeRate);
        cmd.AddValue("backup-bw", "The bandwidth to set on the backup path", backupBandwidth);
        cmd.AddValue("backup-delay", "The delay to set on the backup path", backupDelay);
        cmd.AddValue("backup-rate", "The TCP rate to set to the backup flows", backupRate);
        cmd.AddValue("congestion-control", "The congestion control to use", congestionControl);
        cmd.AddValue("dump", "Dump traffic during the simulation", dumpTraffic);
        cmd.AddValue("verbose", "Verbose output", verbose);
        cmd.Parse(argc, argv);

        LogComponentEnable("LiveLiveExample", LOG_LEVEL_INFO);
        if (verbose)
        {
            LogComponentEnable("FlowMonitor", LOG_LEVEL_DEBUG);
            LogComponentEnable("P4SwitchNetDevice", LOG_LEVEL_DEBUG);
            LogComponentEnable("TcpSocketBase", LOG_LEVEL_DEBUG);
        }

        NS_LOG_INFO("#### RUN PARAMETERS ####");
        NS_LOG_INFO("Dump Traffic: " + std::to_string(dumpTraffic));
        NS_LOG_INFO("Results Path: " + resultsPath);
        NS_LOG_INFO("N. Live-Live Flows: " + std::to_string(llFlows));
        NS_LOG_INFO("N. Active Flows: " + std::to_string(activeFlows));
        NS_LOG_INFO("N. Backup Flows: " + std::to_string(backupFlows));
        NS_LOG_INFO("Default Bandwidth: " + defaultBandwidth);
        NS_LOG_INFO("Active Path Bandwidth: " + activeBandwidth);
        NS_LOG_INFO("Backup Path Bandwidth: " + backupBandwidth);
        NS_LOG_INFO("Live-Live TCP Rate: " + llRate);
        NS_LOG_INFO("Active Flows Rate: " + activeRate);
        NS_LOG_INFO("Backup Flows Rate: " + backupRate);
        NS_LOG_INFO("Active Path Delay: " + std::to_string(activeDelay));
        NS_LOG_INFO("Backup Path Delay: " + std::to_string(backupDelay));
        NS_LOG_INFO("TCP Congestion Control: " + congestionControl);

        std::filesystem::create_directories(resultsPath);

        NodeContainer llSenders;
        llSenders.Create(llFlows);

        NodeContainer activeSenders;
        activeSenders.Create(activeFlows);

        NodeContainer backupSenders;
        backupSenders.Create(backupFlows);

        NodeContainer llReceivers;
        llReceivers.Create(llFlows);

        NodeContainer activeReceivers;
        activeReceivers.Create(activeFlows);

        NodeContainer backupReceivers;
        backupReceivers.Create(backupFlows);

        NodeContainer llSwitches;
        llSwitches.Create(2);

        NodeContainer forwardSwitches;
        forwardSwitches.Create(2);

        Ptr<Node> e1 = llSwitches.Get(0);
        Names::Add("e1", e1);

        Ptr<Node> e2 = llSwitches.Get(1);
        Names::Add("e2", e2);

        Ptr<Node> c1 = forwardSwitches.Get(0);
        Names::Add("c1", c1);

        Ptr<Node> c2 = forwardSwitches.Get(1);
        Names::Add("c2", c2);

        CsmaHelper csma;
        csma.SetChannelAttribute("DataRate", StringValue(defaultBandwidth));

        CsmaHelper csmaActive;
        csmaActive.SetChannelAttribute("DataRate", StringValue(activeBandwidth));
        csmaActive.SetChannelAttribute("Delay", TimeValue(MilliSeconds(activeDelay)));

        CsmaHelper csmaBackup;
        csmaBackup.SetChannelAttribute("DataRate", StringValue(backupBandwidth));
        csmaBackup.SetChannelAttribute("Delay", TimeValue(MilliSeconds(backupDelay)));

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

        NetDeviceContainer link;
        for (uint32_t i = 0; i < llFlows; i++)
        {
            Ptr<Node> llSender = llSenders.Get(i);
            link = csma.Install(NodeContainer(llSender, e1));
            llSenderInterfaces.Add(link.Get(0));
            e1Interfaces.Add(link.Get(1));
        }

        for (uint32_t i = 0; i < activeFlows; i++)
        {
            Ptr<Node> activeSender = activeSenders.Get(i);
            link = csma.Install(NodeContainer(activeSender, e1));
            activeSenderInterfaces.Add(link.Get(0));
            e1Interfaces.Add(link.Get(1));
        }

        for (uint32_t i = 0; i < backupFlows; i++)
        {
            Ptr<Node> backupSender = backupSenders.Get(i);
            link = csma.Install(NodeContainer(backupSender, e1));
            backupSenderInterfaces.Add(link.Get(0));
            e1Interfaces.Add(link.Get(1));
        }

        link = csmaActive.Install(NodeContainer(e1, c1));
        e1Interfaces.Add(link.Get(0));
        c1Interfaces.Add(link.Get(1));

        link = csmaBackup.Install(NodeContainer(e1, c2));
        e1Interfaces.Add(link.Get(0));
        c2Interfaces.Add(link.Get(1));

        link = csma.Install(NodeContainer(c1, e2));
        c1Interfaces.Add(link.Get(0));
        e2Interfaces.Add(link.Get(1));

        link = csma.Install(NodeContainer(c2, e2));
        c2Interfaces.Add(link.Get(0));
        e2Interfaces.Add(link.Get(1));

        for (uint32_t i = 0; i < llFlows; i++)
        {
            Ptr<Node> llReceiver = llReceivers.Get(i);
            link = csma.Install(NodeContainer(llReceiver, e2));
            llReceiverInterfaces.Add(link.Get(0));
            e2Interfaces.Add(link.Get(1));
        }

        for (uint32_t i = 0; i < activeFlows; i++)
        {
            Ptr<Node> activeReceiver = activeReceivers.Get(i);
            link = csma.Install(NodeContainer(activeReceiver, e2));
            activeReceiverInterfaces.Add(link.Get(0));
            e2Interfaces.Add(link.Get(1));
        }

        for (uint32_t i = 0; i < backupFlows; i++)
        {
            Ptr<Node> backupReceiver = backupReceivers.Get(i);
            link = csma.Install(NodeContainer(backupReceiver, e2));
            backupReceiverInterfaces.Add(link.Get(0));
            e2Interfaces.Add(link.Get(1));
        }

        InternetStackHelper internetV6only;
        internetV6only.SetIpv4StackInstall(false);
        internetV6only.Install(activeSenders);
        internetV6only.Install(backupSenders);
        internetV6only.Install(llSenders);
        internetV6only.Install(activeReceivers);
        internetV6only.Install(backupReceivers);
        internetV6only.Install(llReceivers);

        Ipv6AddressHelper llSenderIpv6Helper;
        llSenderIpv6Helper.SetBase(Ipv6Address("2001::"), Ipv6Prefix(64));
        llSenderIpv6Helper.Assign(llSenderInterfaces);

        Ipv6AddressHelper llReceiverIpv6Helper;
        llReceiverIpv6Helper.SetBase(Ipv6Address("2002::"), Ipv6Prefix(64));
        llReceiverIpv6Helper.Assign(llReceiverInterfaces);

        Ipv6AddressHelper activeSenderIpv6Helper;
        activeSenderIpv6Helper.SetBase(Ipv6Address("2003::"), Ipv6Prefix(64));
        activeSenderIpv6Helper.Assign(activeSenderInterfaces);

        Ipv6AddressHelper backupSenderIpv6Helper;
        backupSenderIpv6Helper.SetBase(Ipv6Address("2005::"), Ipv6Prefix(64));
        backupSenderIpv6Helper.Assign(backupSenderInterfaces);

        Ipv6AddressHelper activeReceiverIpv6Helper;
        activeReceiverIpv6Helper.SetBase(Ipv6Address("2004::"), Ipv6Prefix(64));
        activeReceiverIpv6Helper.Assign(activeReceiverInterfaces);

        Ipv6AddressHelper backupReceiverIpv6Helper;
        backupReceiverIpv6Helper.SetBase(Ipv6Address("2006::"), Ipv6Prefix(64));
        backupReceiverIpv6Helper.Assign(backupReceiverInterfaces);

        std::vector<Ptr<Ipv6Interface>> llSenderIpv6Interfaces;
        std::vector<Ptr<Ipv6Interface>> llReceiverIpv6Interfaces;
        for (uint32_t i = 0; i < llFlows; i++)
        {
            Ptr<Ipv6Interface> llSenderIpv6Interface = getIpv6Interface(llSenderInterfaces.Get(i));
            addIpv6Address(llSenderIpv6Interface, &llSenderIpv6Helper);
            llSenderIpv6Interfaces.push_back(llSenderIpv6Interface);

            Ptr<Ipv6Interface> llReceiverIpv6Interface =
                getIpv6Interface(llReceiverInterfaces.Get(i));
            addIpv6Address(llReceiverIpv6Interface, &llReceiverIpv6Helper);
            llReceiverIpv6Interfaces.push_back(llReceiverIpv6Interface);

            addArpEntriesFromInterfaceAddresses(llReceiverIpv6Interface, llSenderIpv6Interface);
            addArpEntriesFromInterfaceAddresses(llSenderIpv6Interface, llReceiverIpv6Interface);
        }

        std::vector<Ptr<Ipv6Interface>> activeSenderIpv6Interfaces;
        std::vector<Ptr<Ipv6Interface>> activeReceiverIpv6Interfaces;
        for (uint32_t i = 0; i < activeFlows; i++)
        {
            Ptr<Ipv6Interface> activeSenderIpv6Interface =
                getIpv6Interface(activeSenderInterfaces.Get(i));
            addIpv6Address(activeSenderIpv6Interface, &activeSenderIpv6Helper);
            activeSenderIpv6Interfaces.push_back(activeSenderIpv6Interface);

            Ptr<Ipv6Interface> activeReceiverIpv6Interface =
                getIpv6Interface(activeReceiverInterfaces.Get(i));
            addIpv6Address(activeReceiverIpv6Interface, &activeReceiverIpv6Helper);
            activeReceiverIpv6Interfaces.push_back(activeReceiverIpv6Interface);

            addArpEntriesFromInterfaceAddresses(activeSenderIpv6Interface,
                                                activeReceiverIpv6Interface);
            addArpEntriesFromInterfaceAddresses(activeReceiverIpv6Interface,
                                                activeSenderIpv6Interface);
        }

        std::vector<Ptr<Ipv6Interface>> backupSenderIpv6Interfaces;
        std::vector<Ptr<Ipv6Interface>> backupReceiverIpv6Interfaces;
        for (uint32_t i = 0; i < backupFlows; i++)
        {
            Ptr<Ipv6Interface> backupSenderIpv6Interface =
                getIpv6Interface(backupSenderInterfaces.Get(i));
            addIpv6Address(backupSenderIpv6Interface, &backupSenderIpv6Helper);
            backupSenderIpv6Interfaces.push_back(backupSenderIpv6Interface);

            Ptr<Ipv6Interface> backupReceiverIpv6Interface =
                getIpv6Interface(backupReceiverInterfaces.Get(i));
            addIpv6Address(backupReceiverIpv6Interface, &backupReceiverIpv6Helper);
            backupReceiverIpv6Interfaces.push_back(backupReceiverIpv6Interface);

            addArpEntriesFromInterfaceAddresses(backupSenderIpv6Interface,
                                                backupReceiverIpv6Interface);
            addArpEntriesFromInterfaceAddresses(backupReceiverIpv6Interface,
                                                backupSenderIpv6Interface);
        }

        P4SwitchHelper liveliveHelper;
        liveliveHelper.SetDeviceAttribute(
            "PipelineJson",
            StringValue("/ns3/ns-3.40/examples/srv6-live-live/livelive_build/srv6_livelive.json"));

        uint8_t mac_str[6];
        std::ostringstream spreaderPortsCommand;

        spreaderPortsCommand << "mc_mgrp_create 1\nmc_node_create 1 "
                             << llFlows + activeFlows + backupFlows + 1 << " "
                             << llFlows + activeFlows + backupFlows + 2 << "\nmc_node_associate 1 0"
                             << std::endl;
        spreaderPortsCommand
            << "table_add check_live_live_enabled live_live_mcast 2001::/64 => 1 e1::2"
            << std::endl;
        spreaderPortsCommand << "table_add srv6_live_live_forward add_srv6_ll_segment 1 => e2::55"
                             << std::endl;

        for (uint32_t i = 0; i < llFlows; i++)
        {
            convertToMacAddress(llSenderIpv6Interfaces[i]->GetDevice()->GetAddress())
                .CopyTo(mac_str);
            spreaderPortsCommand << "table_add ipv6_forward forward 2001::" << i + 1 << "/128 => "
                                 << i + 1 << " " << getMacString(mac_str) << std::endl;
        }

        for (uint32_t i = 0; i < activeFlows; i++)
        {
            convertToMacAddress(activeSenderIpv6Interfaces[i]->GetDevice()->GetAddress())
                .CopyTo(mac_str);
            spreaderPortsCommand << "table_add ipv6_forward forward 2003::" << i + 1 << "/128 => "
                                 << llFlows + i + 1 << " " << getMacString(mac_str) << std::endl;
        }

        for (uint32_t i = 0; i < backupFlows; i++)
        {
            convertToMacAddress(backupSenderIpv6Interfaces[i]->GetDevice()->GetAddress())
                .CopyTo(mac_str);
            spreaderPortsCommand << "table_add ipv6_forward forward 2005::" << i + 1 << "/128 => "
                                 << llFlows + activeFlows + i + 1 << " " << getMacString(mac_str)
                                 << std::endl;
        }

        spreaderPortsCommand
            << "table_set_default check_live_live_enabled ipv6_encap_forward_port e1::2 "
            << llFlows + activeFlows + backupFlows + 1 << std::endl;
        spreaderPortsCommand
            << "table_add check_live_live_enabled ipv6_encap_forward_port 2003::/64 => e1::2 "
            << llFlows + activeFlows + backupFlows + 1 << std::endl;
        spreaderPortsCommand
            << "table_add check_live_live_enabled ipv6_encap_forward_port 2005::/64 => e1::2 "
            << llFlows + activeFlows + backupFlows + 2 << std::endl;
        spreaderPortsCommand << "table_add srv6_forward add_srv6_dest_segment "
                             << llFlows + activeFlows + backupFlows + 1 << " => e2::2" << std::endl;
        spreaderPortsCommand << "table_add srv6_forward add_srv6_dest_segment "
                             << llFlows + activeFlows + backupFlows + 2 << " => e2::2" << std::endl;
        std::string e1Commands =
            spreaderPortsCommand.str() + "table_add srv6_function srv6_ll_deduplicate 85 => \n";
        liveliveHelper.SetDeviceAttribute("PipelineCommands", StringValue(e1Commands));
        liveliveHelper.Install(e1, e1Interfaces);

        std::ostringstream despreaderPortsCommand;
        for (uint32_t i = 0; i < llFlows; i++)
        {
            convertToMacAddress(llReceiverIpv6Interfaces[i]->GetDevice()->GetAddress())
                .CopyTo(mac_str);
            despreaderPortsCommand << "table_add ipv6_forward forward 2002::" << i + 1 << "/128 => "
                                   << 3 + i << " " << getMacString(mac_str) << std::endl;
        }
        for (uint32_t i = 0; i < activeFlows; i++)
        {
            convertToMacAddress(activeReceiverIpv6Interfaces[i]->GetDevice()->GetAddress())
                .CopyTo(mac_str);
            despreaderPortsCommand << "table_add ipv6_forward forward 2004::" << i + 1 << "/128 => "
                                   << 3 + llFlows + i << " " << getMacString(mac_str) << std::endl;
        }
        for (uint32_t i = 0; i < backupFlows; i++)
        {
            convertToMacAddress(backupReceiverIpv6Interfaces[i]->GetDevice()->GetAddress())
                .CopyTo(mac_str);
            despreaderPortsCommand << "table_add ipv6_forward forward 2006::" << i + 1 << "/128 => "
                                   << 3 + llFlows + activeFlows + i << " " << getMacString(mac_str)
                                   << std::endl;
        }

        std::string e2Commands =
            "mc_mgrp_create 1\nmc_node_create 1 1 2\nmc_node_associate 1 0\n"
            "table_add srv6_function srv6_ll_deduplicate 85 => \n"
            "table_add check_live_live_enabled live_live_mcast 2002::/64 => 1 e2::2\n"
            "table_add check_live_live_enabled ipv6_encap_forward_port 2004::/64 => e2::2 1\n"
            "table_add check_live_live_enabled ipv6_encap_forward_port 2006::/64 => e2::2 2\n"
            "table_add srv6_forward add_srv6_dest_segment 1 => e1::2\n"
            "table_add srv6_forward add_srv6_dest_segment 2 => e1::2\n"
            "table_add srv6_live_live_forward add_srv6_ll_segment 1 => e1::55\n" +
            despreaderPortsCommand.str();
        liveliveHelper.SetDeviceAttribute("PipelineCommands", StringValue(e2Commands));
        liveliveHelper.Install(e2, e2Interfaces);

        if (verbose)
        {
            NS_LOG_INFO("e1 COMMANDS:");
            NS_LOG_INFO(e1Commands);

            NS_LOG_INFO("e2 COMMANDS:");
            NS_LOG_INFO(e2Commands);
        }

        P4SwitchHelper forwardHelper;
        forwardHelper.SetDeviceAttribute(
            "PipelineJson",
            StringValue("/ns3/ns-3.40/examples/srv6-live-live/forward_build/srv6_forward.json"));
        forwardHelper.SetDeviceAttribute(
            "PipelineCommands",
            StringValue("table_add srv6_table srv6_noop 2002::/64 => 2\n"
                        "table_add srv6_table srv6_seg_ep  e2::2/128 => 2\n"
                        "table_add srv6_table srv6_noop 2001::/64 => 1\n"
                        "table_add srv6_table srv6_seg_ep  e1::2/128 => 1"));
        forwardHelper.Install(c1, c1Interfaces);
        forwardHelper.Install(c2, c2Interfaces);

        NS_LOG_INFO("Configuring Congestion Control.");
        Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue(congestionControl));

        NS_LOG_INFO("Create Applications.");
        uint16_t activePort = 20000;
        NS_LOG_INFO("Create Active Flow Applications.");
        for (uint32_t i = 0; i < activeFlows; i++)
        {
            ApplicationContainer activeReceiverApp =
                createSinkTcpApplication(activePort + i, activeReceivers.Get(i));
            activeReceiverApp.Start(Seconds(0.0));
            activeReceiverApp.Stop(Seconds(11.0));

            ApplicationContainer activeSenderApp = createOnOffTcpApplication(
                activeReceiverIpv6Interfaces[i]->GetAddress(2).GetAddress(),
                activePort + i,
                activeSenders.Get(i),
                activeRate,
                0);
            activeSenderApp.Start(Seconds(1.0));
            activeSenderApp.Stop(Seconds(10.0));
        }

        uint16_t backupPort = 30000;
        NS_LOG_INFO("Create Backup Flow Applications.");
        for (uint32_t i = 0; i < backupFlows; i++)
        {
            ApplicationContainer backupReceiverApp =
                createSinkTcpApplication(backupPort + i, backupReceivers.Get(i));
            backupReceiverApp.Start(Seconds(0.0));
            backupReceiverApp.Stop(Seconds(11.0));

            ApplicationContainer backupSenderApp = createOnOffTcpApplication(
                backupReceiverIpv6Interfaces[i]->GetAddress(2).GetAddress(),
                backupPort + i,
                backupSenders.Get(i),
                backupRate,
                0);
            backupSenderApp.Start(Seconds(1.0));
            backupSenderApp.Stop(Seconds(10.0));
        }

        uint16_t llPort = 40000;
        for (uint32_t i = 0; i < llFlows; i++)
        {
            ApplicationContainer llReceiverApp =
                createSinkTcpApplication(llPort + i, llReceivers.Get(i));
            llReceiverApp.Start(Seconds(0.0));
            llReceiverApp.Stop(Seconds(11.0));

            ApplicationContainer llSenderApp =
                createOnOffTcpApplication(llReceiverIpv6Interfaces[i]->GetAddress(2).GetAddress(),
                                          llPort + i,
                                          llSenders.Get(i),
                                          llRate,
                                          0);
            llSenderApp.Start(Seconds(1.0));
            llSenderApp.Stop(Seconds(10.0));
        }

        NS_LOG_INFO("Configure Tracing.");
        AsciiTraceHelper ascii;

        for (uint32_t i = 0; i < llFlows; i++)
        {
            std::ostringstream file_name;
            file_name << "llsender-" << i << "-cwnd.data";
            std::string directory_path = get_path(resultsPath, "cwnd");
            std::string file_path = get_path(directory_path, file_name.str());
            Simulator::Schedule(Seconds(2), &TraceCwnd, file_path, llSenders.Get(i)->GetId());
        }

        for (uint32_t i = 0; i < activeFlows; i++)
        {
            std::ostringstream file_name;
            file_name << "active-sender-" << i << "-cwnd.data";
            std::string directory_path = get_path(resultsPath, "cwnd");
            std::string file_path = get_path(directory_path, file_name.str());
            Simulator::Schedule(Seconds(2), &TraceCwnd, file_path, activeSenders.Get(i)->GetId());
        }

        for (uint32_t i = 0; i < backupFlows; i++)
        {
            std::ostringstream file_name;
            file_name << "backup-sender-" << i << "-cwnd.data";
            std::string directory_path = get_path(resultsPath, "cwnd");
            std::string file_path = get_path(directory_path, file_name.str());
            Simulator::Schedule(Seconds(2), &TraceCwnd, file_path, backupSenders.Get(i)->GetId());
        }

        if (dumpTraffic)
        {
            csma.EnableAsciiAll(ascii.CreateFileStream(get_path(resultsPath, "p4-switch.tr")));
            csma.EnablePcapAll(get_path(resultsPath, "p4-switch"), true);

            csmaActive.EnableAsciiAll(
                ascii.CreateFileStream(get_path(resultsPath, "p4-switch.tr")));
            csmaActive.EnablePcapAll(get_path(resultsPath, "p4-switch"), true);

            csmaBackup.EnableAsciiAll(
                ascii.CreateFileStream(get_path(resultsPath, "p4-switch.tr")));
            csmaBackup.EnablePcapAll(get_path(resultsPath, "p4-switch"), true);
        }

        FlowMonitorHelper flowHelper;
        Ptr<FlowMonitor> flowMon = flowHelper.Install(NodeContainer(activeSenders,
                                                                    activeReceivers,
                                                                    backupSenders,
                                                                    backupReceivers,
                                                                    llSenders,
                                                                    llReceivers));

        NS_LOG_INFO("Run Simulation.");
        Simulator::Stop(Seconds(10));
        Simulator::Run();
        flowMon->CheckForLostPackets();
        flowMon->SerializeToXmlFile(get_path(resultsPath, "flow_monitor.xml"), true, true);
        Simulator::Destroy();
        NS_LOG_INFO("Done.");
    }
