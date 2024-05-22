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
#include <random>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LiveLiveExample");

bool verbose = false;

uint32_t SEED = 10;
std::mt19937 randomGen;
std::uniform_real_distribution distribution;

std::string
getMacString(uint8_t* mac)
{
    std::string s = "0x";
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (uint32_t i = 0; i < 6; i++)
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
        if (verbose)
        {
            NS_LOG_INFO("Add ARP Entry for " << "(" << ipv6Address << ", " << macAddress << ")");
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
    OnOffHelper onoff("ns3::TcpSocketFactory", Address(Inet6SocketAddress(addressToReach, port)));
    onoff.SetConstantRate(DataRate(data_rate), 512);
    //    onoff.SetAttribute("DataRate", StringValue(data_rate));
    onoff.SetAttribute("MaxBytes", UintegerValue(max_bytes));
    //    onoff.SetAttribute("PacketSize", UintegerValue(1500));

    ApplicationContainer senderApp = onoff.Install(node);
    return senderApp;
}

ApplicationContainer
createOnOffUdpApplication(Ipv6Address addressToReach,
                          uint16_t port,
                          Ptr<Node> node,
                          std::string data_rate,
                          uint32_t flowEndTime,
                          uint32_t max_bytes,
                          bool generateRandom)
{
    OnOffHelper onoff("ns3::UdpSocketFactory", Address(Inet6SocketAddress(addressToReach, port)));
    onoff.SetConstantRate(DataRate(data_rate), 512);
    //    onoff.SetAttribute("DataRate", StringValue(data_rate));
    onoff.SetAttribute("MaxBytes", UintegerValue(max_bytes));
    //    onoff.SetAttribute("PacketSize", UintegerValue(1500));

    ApplicationContainer senderApp = onoff.Install(node);

    double startTime = 0.0;
    double endTime = flowEndTime;
    if (generateRandom)
    {
        startTime = distribution(randomGen);
        std::uniform_real_distribution endDistribution(startTime + 0.3, (double)flowEndTime);
        endTime = endDistribution(randomGen);
    }
    NS_LOG_INFO("UDP Application: " + std::to_string(startTime) + " - " + std::to_string(endTime));
    senderApp.Start(Seconds(startTime));
    senderApp.Stop(Seconds(endTime));

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

ApplicationContainer
createSinkUdpApplication(uint16_t port, Ptr<Node> node)
{
    PacketSinkHelper sink("ns3::UdpSocketFactory",
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
getPath(std::string directory, std::string file)
{
    return SystemPath::Append(directory, file);
}

static std::map<uint32_t, bool> firstCwnd;                      //!< First congestion window.
static std::map<uint32_t, Ptr<OutputStreamWrapper>> cWndStream; //!< Congstion window output stream.
static std::map<uint32_t, uint32_t> cWndValue;                  //!< congestion window value.

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
    *cWndStream[nodeId]->GetStream() << Simulator::Now().GetSeconds() << " " << newval << std::endl;
    cWndValue[nodeId] = newval;
}

static void
TraceCwnd(std::string fileName, uint32_t nodeId)
{
    AsciiTraceHelper ascii;
    auto it = cWndStream.find(nodeId);
    if (it == cWndStream.end())
        cWndStream[nodeId] = ascii.CreateFileStream(fileName);

    Config::Connect("/NodeList/" + std::to_string(nodeId) +
                        "/$ns3::TcpL4Protocol/SocketList/0/CongestionWindow",
                    MakeCallback(&CwndTracer));
}

std::map<std::string, std::pair<uint64_t, uint64_t>> ctx2tpInfo;
std::map<std::string, Ptr<OutputStreamWrapper>> tpStream;
Time period = Time::FromInteger(500, Time::Unit::MS);

void
trackePktReceivedNetDevice(std::string context, Ptr<const Packet> p)
{
    uint64_t lastTs = Simulator::Now().GetNanoSeconds();
    uint32_t pktSize = p->GetSize() * 8;

    auto ctxIt = ctx2tpInfo.find(context);
    if (ctxIt == ctx2tpInfo.end())
    {
        /* First entry for the context, store the current time and the first size in bits */
        ctx2tpInfo.insert(std::make_pair(context, std::make_pair(lastTs, pktSize)));
    }
    else
    {
        /* An entry already exists, check if we have reached the interval */
        Time interval(lastTs - (*ctxIt).second.first);
        (*ctxIt).second.second += pktSize;
        if (interval.Compare(period) >= 0)
        {
            /* Yes, compute the bps and store it */
            double bps = (*ctxIt).second.second * (1000000 / interval.GetMicroSeconds());
            *tpStream[context]->GetStream() << std::fixed;
            *tpStream[context]->GetStream()
                << std::setprecision(2) << Simulator::Now().GetSeconds() << " " << bps << std::endl;
            /* Delete this entry so it can restart for the next period */
            ctx2tpInfo.erase(ctxIt);
        }
    }
}

void
startThroughputTrace(std::string fileName, uint32_t nodeId, uint32_t ifaceId)
{
    std::string nsString = "/NodeList/" + std::to_string(nodeId) + "/DeviceList/" +
                           std::to_string(ifaceId) + "/$ns3::CsmaNetDevice/MacTx";

    AsciiTraceHelper ascii;
    auto it = tpStream.find(nsString);
    if (it == tpStream.end())
        tpStream[nsString] = ascii.CreateFileStream(fileName);

    Config::Connect(nsString, MakeCallback(&trackePktReceivedNetDevice));
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
    float flowEndTime = 11.0f;
    float endTime = 20.0f;
    bool dumpTraffic = false;
    std::string defaultBuffer = "1000p";
    std::string activeBuffer = "1000p";
    std::string backupBuffer = "1000p";
    bool generateRandom = false;

    CommandLine cmd;
    cmd.AddValue("results-path", "The path where to save results", resultsPath);
    cmd.AddValue("ll-flows", "The number of concurrent live-live flows to generate", llFlows);
    cmd.AddValue("active-flows", "The number of concurrent flows on the active path", activeFlows);
    cmd.AddValue("backup-flows", "The number of concurrent flows on the backup path", backupFlows);
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
    cmd.AddValue("flow-end", "Flows End Time", flowEndTime);
    cmd.AddValue("end", "Simulation End Time", endTime);
    cmd.AddValue("default-buffer", "The size of the default buffers", defaultBuffer);
    cmd.AddValue("active-buffer", "The size of the active buffers", activeBuffer);
    cmd.AddValue("backup-buffer", "The size of the backup buffers", backupBuffer);
    cmd.AddValue("random", "Select whether UDP flows are randomly distributed.", generateRandom);
    cmd.AddValue("seed", "The seed used for the simulation", SEED);
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
    NS_LOG_INFO("Default Buffer Size: " + defaultBuffer);
    NS_LOG_INFO("Active Buffer Size: " + activeBuffer);
    NS_LOG_INFO("Backup Buffer Size: " + backupBuffer);
    NS_LOG_INFO("TCP Congestion Control: " + congestionControl);
    NS_LOG_INFO("Flow End Time: " + std::to_string(flowEndTime));
    NS_LOG_INFO("End Time: " + std::to_string(endTime));

    std::filesystem::create_directories(resultsPath);

    randomGen = std::mt19937(SEED);
    distribution = std::uniform_real_distribution(0.0, (double)flowEndTime);

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

    //    Config::SetDefault("ns3::DropTailQueue::MaxPackets", UintegerValue(1));
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue(defaultBandwidth));
    csma.SetQueue("ns3::DropTailQueue<Packet>", "MaxSize", StringValue(defaultBuffer));

    CsmaHelper csmaActive;
    csmaActive.SetChannelAttribute("DataRate", StringValue(activeBandwidth));
    csmaActive.SetChannelAttribute("Delay", TimeValue(MilliSeconds(activeDelay)));
    csmaActive.SetQueue("ns3::DropTailQueue", "MaxSize", StringValue(activeBuffer));

    CsmaHelper csmaBackup;
    csmaBackup.SetChannelAttribute("DataRate", StringValue(backupBandwidth));
    csmaBackup.SetChannelAttribute("Delay", TimeValue(MilliSeconds(backupDelay)));
    csmaBackup.SetQueue("ns3::DropTailQueue", "MaxSize", StringValue(backupBuffer));

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

        Ptr<Ipv6Interface> llReceiverIpv6Interface = getIpv6Interface(llReceiverInterfaces.Get(i));
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

        addArpEntriesFromInterfaceAddresses(activeSenderIpv6Interface, activeReceiverIpv6Interface);
        addArpEntriesFromInterfaceAddresses(activeReceiverIpv6Interface, activeSenderIpv6Interface);
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

        addArpEntriesFromInterfaceAddresses(backupSenderIpv6Interface, backupReceiverIpv6Interface);
        addArpEntriesFromInterfaceAddresses(backupReceiverIpv6Interface, backupSenderIpv6Interface);
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
    spreaderPortsCommand << "table_add check_live_live_enabled live_live_mcast 2001::/64 => 1 e1::2"
                         << std::endl;
    spreaderPortsCommand << "table_add srv6_live_live_forward add_srv6_ll_segment 1 => e2::55"
                         << std::endl;

    for (uint32_t i = 0; i < llFlows; i++)
    {
        convertToMacAddress(llSenderIpv6Interfaces[i]->GetDevice()->GetAddress()).CopyTo(mac_str);

        Ipv6Address addr = llSenderIpv6Interfaces[i]->GetAddress(2).GetAddress();
        spreaderPortsCommand << "table_add ipv6_forward forward " << addr << "/128 => " << i + 1
                             << " " << getMacString(mac_str) << std::endl;
    }

    for (uint32_t i = 0; i < activeFlows; i++)
    {
        convertToMacAddress(activeSenderIpv6Interfaces[i]->GetDevice()->GetAddress())
            .CopyTo(mac_str);

        Ipv6Address addr = activeSenderIpv6Interfaces[i]->GetAddress(2).GetAddress();
        spreaderPortsCommand << "table_add ipv6_forward forward " << addr << "/128 => "
                             << llFlows + i + 1 << " " << getMacString(mac_str) << std::endl;
    }

    for (uint32_t i = 0; i < backupFlows; i++)
    {
        convertToMacAddress(backupSenderIpv6Interfaces[i]->GetDevice()->GetAddress())
            .CopyTo(mac_str);

        Ipv6Address addr = backupSenderIpv6Interfaces[i]->GetAddress(2).GetAddress();
        spreaderPortsCommand << "table_add ipv6_forward forward " << addr << "/128 => "
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
        convertToMacAddress(llReceiverIpv6Interfaces[i]->GetDevice()->GetAddress()).CopyTo(mac_str);

        Ipv6Address addr = llReceiverIpv6Interfaces[i]->GetAddress(2).GetAddress();
        despreaderPortsCommand << "table_add ipv6_forward forward " << addr << "/128 => " << 3 + i
                               << " " << getMacString(mac_str) << std::endl;
    }
    for (uint32_t i = 0; i < activeFlows; i++)
    {
        convertToMacAddress(activeReceiverIpv6Interfaces[i]->GetDevice()->GetAddress())
            .CopyTo(mac_str);

        Ipv6Address addr = activeReceiverIpv6Interfaces[i]->GetAddress(2).GetAddress();
        despreaderPortsCommand << "table_add ipv6_forward forward " << addr << "/128 => "
                               << 3 + llFlows + i << " " << getMacString(mac_str) << std::endl;
    }
    for (uint32_t i = 0; i < backupFlows; i++)
    {
        convertToMacAddress(backupReceiverIpv6Interfaces[i]->GetDevice()->GetAddress())
            .CopyTo(mac_str);

        Ipv6Address addr = backupReceiverIpv6Interfaces[i]->GetAddress(2).GetAddress();
        despreaderPortsCommand << "table_add ipv6_forward forward " << addr << "/128 => "
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

    if (activeFlows > 0)
    {
        ApplicationContainer activeReceiverApp =
            createSinkTcpApplication(activePort, activeReceivers.Get(0));
        activeReceiverApp.Start(Seconds(0.0));
        activeReceiverApp.Stop(Seconds(flowEndTime + 1));

        ApplicationContainer activeSenderApp =
            createOnOffTcpApplication(activeReceiverIpv6Interfaces[0]->GetAddress(2).GetAddress(),
                                      activePort,
                                      activeSenders.Get(0),
                                      activeRate,
                                      0);
        activeSenderApp.Start(Seconds(1.0));
        activeSenderApp.Stop(Seconds(flowEndTime));

        for (uint32_t i = 1; i < activeFlows; i++)
        {
            activeReceiverApp = createSinkUdpApplication(activePort + i, activeReceivers.Get(i));
            activeReceiverApp.Start(Seconds(0.0));
            activeReceiverApp.Stop(Seconds(flowEndTime + 1));

            activeSenderApp = createOnOffUdpApplication(
                activeReceiverIpv6Interfaces[i]->GetAddress(2).GetAddress(),
                activePort + i,
                activeSenders.Get(i),
                activeRate,
                flowEndTime,
                0,
                generateRandom);
        }
    }

    uint16_t backupPort = 30000;
    NS_LOG_INFO("Create Backup Flow Applications.");
    if (backupFlows > 0)
    {
        ApplicationContainer backupReceiverApp =
            createSinkTcpApplication(backupPort, backupReceivers.Get(0));
        backupReceiverApp.Start(Seconds(0.0));
        backupReceiverApp.Stop(Seconds(flowEndTime + 1));

        ApplicationContainer backupSenderApp =
            createOnOffTcpApplication(backupReceiverIpv6Interfaces[0]->GetAddress(2).GetAddress(),
                                      backupPort,
                                      backupSenders.Get(0),
                                      backupRate,
                                      0);
        backupSenderApp.Start(Seconds(1.0));
        backupSenderApp.Stop(Seconds(flowEndTime));

        for (uint32_t i = 1; i < backupFlows; i++)
        {
            backupReceiverApp = createSinkUdpApplication(backupPort + i, backupReceivers.Get(i));
            backupReceiverApp.Start(Seconds(0.0));
            backupReceiverApp.Stop(Seconds(flowEndTime + 1));

            backupSenderApp = createOnOffUdpApplication(
                backupReceiverIpv6Interfaces[i]->GetAddress(2).GetAddress(),
                backupPort + i,
                backupSenders.Get(i),
                backupRate,
                flowEndTime,
                0,
                generateRandom);
        }
    }

    uint16_t llPort = 40000;
    if (llFlows > 0)
    {
        for (uint32_t i = 0; i < llFlows; i++)
        {
            ApplicationContainer llReceiverApp =
                createSinkTcpApplication(llPort + i, llReceivers.Get(i));
            llReceiverApp.Start(Seconds(0.0));
            llReceiverApp.Stop(Seconds(flowEndTime + 1));

            ApplicationContainer llSenderApp =
                createOnOffTcpApplication(llReceiverIpv6Interfaces[i]->GetAddress(2).GetAddress(),
                                          llPort + i,
                                          llSenders.Get(i),
                                          llRate,
                                          0);
            llSenderApp.Start(Seconds(1.0));
            llSenderApp.Stop(Seconds(flowEndTime));
        }
    }

    /* Add Bandwidth monitor only on the interface 0 of c1 and c2 */
    std::string tpPath = getPath(resultsPath, "throughput");
    std::filesystem::create_directories(tpPath);
    Simulator::Schedule(Seconds(0),
                        &startThroughputTrace,
                        getPath(tpPath, "e1-0-tp.data"),
                        e1->GetId(),
                        llFlows + activeFlows + backupFlows);
    Simulator::Schedule(Seconds(0),
                        &startThroughputTrace,
                        getPath(tpPath, "e1-1-tp.data"),
                        e1->GetId(),
                        llFlows + activeFlows + backupFlows + 1);

    NS_LOG_INFO("Configure Tracing.");
    AsciiTraceHelper ascii;

    std::string cwndPath = getPath(resultsPath, "cwnd");
    std::filesystem::create_directories(cwndPath);

    for (uint32_t i = 0; i < llFlows; i++)
    {
        std::string path = getPath(cwndPath, "ll-sender-" + std::to_string(i) + "-cwnd.data");
        Simulator::Schedule(Seconds(2), &TraceCwnd, path, llSenders.Get(i)->GetId());
    }

    std::string path = getPath(cwndPath, "active-sender-" + std::to_string(0) + "-cwnd.data");
    Simulator::Schedule(Seconds(2), &TraceCwnd, path, activeSenders.Get(0)->GetId());

    path = getPath(cwndPath, "backup-sender-" + std::to_string(0) + "-cwnd.data");
    Simulator::Schedule(Seconds(2), &TraceCwnd, path, backupSenders.Get(0)->GetId());

    if (dumpTraffic)
    {
        std::string tracesPath = getPath(resultsPath, "traces");
        std::filesystem::create_directories(tracesPath);

        csma.EnableAsciiAll(ascii.CreateFileStream(getPath(tracesPath, "p4-switch.tr")));
        csma.EnablePcapAll(getPath(tracesPath, "p4-switch"), true);

        csmaActive.EnableAsciiAll(ascii.CreateFileStream(getPath(tracesPath, "p4-switch.tr")));
        csmaActive.EnablePcapAll(getPath(tracesPath, "p4-switch"), true);

        csmaBackup.EnableAsciiAll(ascii.CreateFileStream(getPath(tracesPath, "p4-switch.tr")));
        csmaBackup.EnablePcapAll(getPath(tracesPath, "p4-switch"), true);
    }

    FlowMonitorHelper flowHelper;
    Ptr<FlowMonitor> flowMon = flowHelper.Install(NodeContainer(activeSenders,
                                                                activeReceivers,
                                                                backupSenders,
                                                                backupReceivers,
                                                                llSenders,
                                                                llReceivers));

    NS_LOG_INFO("Run Simulation.");
    Simulator::Stop(Seconds(endTime));
    Simulator::Run();
    flowMon->CheckForLostPackets();

    std::string flowMonitorPath = getPath(resultsPath, "flow-monitor");
    std::filesystem::create_directories(flowMonitorPath);
    flowMon->SerializeToXmlFile(getPath(flowMonitorPath, "flow_monitor.xml"), true, true);

    Simulator::Destroy();
    NS_LOG_INFO("Done.");
}
