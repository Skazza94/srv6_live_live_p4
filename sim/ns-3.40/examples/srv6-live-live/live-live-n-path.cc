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
createTcpApplication(Ipv6Address addressToReach,
                     uint16_t port,
                     Ptr<Node> node,
                     std::string dataRate,
                     uint32_t maxBytes,
                     std::string congestionControl)
{
    TypeId congestionControlTid = TypeId::LookupByName(congestionControl);

    Config::Set("/NodeList/" + std::to_string(node->GetId()) + "/$ns3::TcpL4Protocol/SocketType",
                TypeIdValue(congestionControlTid));

    OnOffHelper source("ns3::TcpSocketFactory", Address(Inet6SocketAddress(addressToReach, port)));
    source.SetConstantRate(DataRate(dataRate), 1400);
    source.SetAttribute("MaxBytes", UintegerValue(maxBytes));

    return source.Install(node);
}

ApplicationContainer
createSinkTcpApplication(uint16_t port, Ptr<Node> node)
{
    PacketSinkHelper sink("ns3::TcpSocketFactory",
                          Address(Inet6SocketAddress(Ipv6Address::GetAny(), port)));
    return sink.Install(node);
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

std::map<uint32_t, bool> firstCwnd;                      //!< First congestion window.
std::map<uint32_t, Ptr<OutputStreamWrapper>> cWndStream; //!< Congstion window output stream.
std::map<uint32_t, uint32_t> cWndValue;                  //!< congestion window value.

uint32_t
GetNodeIdFromContext(std::string context)
{
    const std::size_t n1 = context.find_first_of('/', 1);
    const std::size_t n2 = context.find_first_of('/', n1 + 1);
    return std::stoul(context.substr(n1 + 1, n2 - n1 - 1));
}

void
CwndTracer(std::string context, uint32_t oldval, uint32_t newval)
{
    uint32_t nodeId = GetNodeIdFromContext(context);

    if (firstCwnd[nodeId])
    {
        *cWndStream[nodeId]->GetStream() << "0.0 " << oldval << std::endl;
        firstCwnd[nodeId] = false;
    }
    *cWndStream[nodeId]->GetStream() << Simulator::Now().GetSeconds() << " " << newval << std::endl;
    *cWndStream[nodeId]->GetStream() << std::flush;
    cWndValue[nodeId] = newval;
}

void
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
std::map<std::string, FILE*> tpStream;
Time period = Time::FromInteger(100, Time::Unit::MS);

void
tracePktTxNetDevice(std::string context, Ptr<const Packet> p)
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
            fprintf(tpStream[context], "%f %f\n", Simulator::Now().GetSeconds(), bps);
            fflush(tpStream[context]);
            fsync(fileno(tpStream[context]));

            /* Delete this entry so it can restart for the next period */
            ctx2tpInfo.erase(ctxIt);
        }
    }
}

void
startThroughputTrace(std::string fileName, uint32_t nodeId, uint32_t ifaceId)
{
    std::string nsString = "/NodeList/" + std::to_string(nodeId) + "/DeviceList/" +
                           std::to_string(ifaceId) + "/$ns3::CsmaNetDevice/MacRx";

    auto it = tpStream.find(nsString);
    if (it == tpStream.end())
        tpStream[nsString] = fopen(fileName.c_str(), "w");

    Config::Connect(nsString, MakeCallback(&tracePktTxNetDevice));
}

/* Functions to track TCP Retransmissions */
std::map<std::string, std::pair<SequenceNumber32, uint32_t>> ctx2rtxInfo;
std::map<std::string, FILE*> rtxStream;
Time rtxPeriod = Time::FromInteger(100, Time::Unit::MS);

void
tcpRx(std::string context,
      const Ptr<const Packet> p,
      const TcpHeader& hdr,
      const Ptr<const TcpSocketBase> skt)
{
    auto it = ctx2rtxInfo.find(context);
    if (it == ctx2rtxInfo.end())
        return;

    SequenceNumber32 currSeqno = hdr.GetSequenceNumber();

    if (currSeqno <= (*it).second.first)
    {
        (*it).second.second++;
        fprintf(rtxStream[context], "%f %d\n", Simulator::Now().GetSeconds(), (*it).second.second);
        fflush(rtxStream[context]);
        fsync(fileno(rtxStream[context]));
    }
    else
    {
        (*it).second.first = currSeqno;
    }
}

void
startTcpRtx(uint32_t nodeId, std::string fileName)
{
    std::string nsString =
        "/NodeList/" + std::to_string(nodeId) + "/$ns3::TcpL4Protocol/SocketList/1/Rx";

    auto fileIt = rtxStream.find(nsString);
    if (fileIt == rtxStream.end())
        rtxStream[nsString] = fopen(fileName.c_str(), "w");

    auto seqnoIt = ctx2rtxInfo.find(nsString);
    if (seqnoIt == ctx2rtxInfo.end())
    {
        SequenceNumber32 prevSeqno(0);
        ctx2rtxInfo[nsString] = std::make_pair(prevSeqno, 0);
    }

    Config::Connect(nsString, MakeCallback(&tcpRx));
}

int
main(int argc, char* argv[])
{
    uint32_t llFlows = 1;
    std::string resultsPath = "examples/srv6-live-live/results/";
    std::string defaultBandwidth = "50Kbps";
    std::string llRate = "50Kbps";
    std::string pathBandwidth = "50Kbps";
    std::string pathDelay = "0us";
    std::string congestionControl = "TcpLinuxReno";
    float endTime = 20.0f;
    bool dumpTraffic = false;
    std::string defaultBuffer = "1000p";
    std::string pathBuffer = "1000p";
    uint32_t maxBytes = 15000000;
    uint32_t nPaths = 2;
    std::string testType = "live-live";
    uint32_t seed = 10;

    CommandLine cmd;
    cmd.AddValue("results-path", "The path where to save results", resultsPath);
    cmd.AddValue("ll-flows", "The number of concurrent live-live flows to generate", llFlows);
    cmd.AddValue("default-bw",
                 "The bandwidth to set on all the sender/receiver links",
                 defaultBandwidth);
    cmd.AddValue("ll-rate", "The TCP rate to set to the live-live flows", llRate);
    cmd.AddValue("max-bytes", "Bytes to send from TCP applications", maxBytes);
    cmd.AddValue("path-bw", "The bandwidth to set on the N paths", pathBandwidth);
    cmd.AddValue("path-delay", "The delay to set on the N paths", pathDelay);
    cmd.AddValue("congestion-control", "The congestion control to use", congestionControl);
    cmd.AddValue("end", "Simulation End Time", endTime);
    cmd.AddValue("default-buffer", "The size of the default buffers", defaultBuffer);
    cmd.AddValue("path-buffer", "The size of the N paths buffers", pathBuffer);
    cmd.AddValue("seed", "The seed used for the simulation", seed);
    cmd.AddValue("n-paths", "Number of alternative paths", nPaths);
    cmd.AddValue("test-type", "Test type", testType);
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
    NS_LOG_INFO("Default Bandwidth: " + defaultBandwidth);
    NS_LOG_INFO("N Path Bandwidth: " + pathBandwidth);
    NS_LOG_INFO("Live-Live TCP Rate: " + llRate);
    NS_LOG_INFO("TCP Bytes to Send: " + std::to_string(maxBytes));
    NS_LOG_INFO("N Path Delay: " + pathDelay);
    NS_LOG_INFO("Default Buffer Size: " + defaultBuffer);
    NS_LOG_INFO("N Path Buffer Size: " + pathBuffer);
    NS_LOG_INFO("TCP Congestion Control: " + congestionControl);
    NS_LOG_INFO("End Time: " + std::to_string(endTime));

    NS_LOG_INFO("Configuring Congestion Control.");
    Config::SetDefault("ns3::TcpSocket::SndBufSize", UintegerValue(2 << 17));
    Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue(2 << 17));
    Config::SetDefault("ns3::TcpSocket::InitialCwnd", UintegerValue(10));
    Config::SetDefault("ns3::TcpSocket::DelAckCount", UintegerValue(2));
    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(1400));

    std::filesystem::create_directories(resultsPath);

    std::mt19937 generator(seed);
    std::lognormal_distribution<double> distribution(1, 0.7);

    NodeContainer llSenders;
    llSenders.Create(llFlows);

    NodeContainer llReceivers;
    llReceivers.Create(llFlows);

    NodeContainer llSwitches;
    llSwitches.Create(2);

    NodeContainer forwardSwitches;
    forwardSwitches.Create(nPaths);

    Ptr<Node> e1 = llSwitches.Get(0);
    Names::Add("e1", e1);

    Ptr<Node> e2 = llSwitches.Get(1);
    Names::Add("e2", e2);

    for (uint32_t i = 0; i < nPaths; ++i)
    {
        Ptr<Node> n = forwardSwitches.Get(i);
        Names::Add("c" + std::to_string(i + 1), n);
    }

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue(defaultBandwidth));
    csma.SetDeviceAttribute("Mtu", UintegerValue(1500));

    NetDeviceContainer llSenderInterfaces;
    NetDeviceContainer llReceiverInterfaces;
    NetDeviceContainer e1Interfaces;
    NetDeviceContainer e2Interfaces;

    std::vector<CsmaHelper> pathsCsma;
    std::vector<NetDeviceContainer> pathsNodesNetDevices;

    NetDeviceContainer link;
    for (uint32_t i = 0; i < llFlows; i++)
    {
        Ptr<Node> llSender = llSenders.Get(i);
        link = csma.Install(NodeContainer(llSender, e1));
        llSenderInterfaces.Add(link.Get(0));
        e1Interfaces.Add(link.Get(1));
    }

    for (uint32_t i = 0; i < nPaths; ++i)
    {
        Ptr<Node> n = forwardSwitches.Get(i);

        CsmaHelper csmaPath;
        csmaPath.SetChannelAttribute("DataRate", StringValue(pathBandwidth));
        csmaPath.SetChannelAttribute("Delay", TimeValue(Time(pathDelay)));
        csmaPath.SetDeviceAttribute("Mtu", UintegerValue(1500));
        csmaPath.SetQueue("ns3::DropTailQueue", "MaxSize", StringValue(pathBuffer));

        double value = distribution(generator) / 100.0f;
        NS_LOG_INFO("Node c" << (i + 1) << " loss: " << value);

        Ptr<RateErrorModel> rem = CreateObject<RateErrorModel>();
        Ptr<UniformRandomVariable> uv = CreateObject<UniformRandomVariable>();
        rem->SetRandomVariable(uv);
        uv->SetStream(seed);
        rem->SetAttribute("ErrorRate", DoubleValue(value));
        rem->SetAttribute("ErrorUnit", StringValue("ERROR_UNIT_PACKET"));
        csmaPath.SetDeviceAttribute("ReceiveErrorModel", PointerValue(rem));

        NetDeviceContainer nInterfaces;

        link = csmaPath.Install(NodeContainer(e1, n));
        e1Interfaces.Add(link.Get(0));
        nInterfaces.Add(link.Get(1));

        link = csmaPath.Install(NodeContainer(n, e2));
        nInterfaces.Add(link.Get(0));
        e2Interfaces.Add(link.Get(1));

        pathsCsma.push_back(csmaPath);
        pathsNodesNetDevices.push_back(nInterfaces);
    }

    for (uint32_t i = 0; i < llFlows; i++)
    {
        Ptr<Node> llReceiver = llReceivers.Get(i);
        link = csma.Install(NodeContainer(llReceiver, e2));
        llReceiverInterfaces.Add(link.Get(0));
        e2Interfaces.Add(link.Get(1));
    }

    InternetStackHelper internetV6only;
    internetV6only.SetIpv4StackInstall(false);
    internetV6only.Install(llSenders);
    internetV6only.Install(llReceivers);

    Ipv6AddressHelper llSenderIpv6Helper;
    llSenderIpv6Helper.SetBase(Ipv6Address("2001::"), Ipv6Prefix(64));
    llSenderIpv6Helper.Assign(llSenderInterfaces);

    Ipv6AddressHelper llReceiverIpv6Helper;
    llReceiverIpv6Helper.SetBase(Ipv6Address("2002::"), Ipv6Prefix(64));
    llReceiverIpv6Helper.Assign(llReceiverInterfaces);

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

    P4SwitchHelper liveliveHelper;
    liveliveHelper.SetDeviceAttribute(
        "PipelineJson",
        StringValue("/ns3/ns-3.40/examples/srv6-live-live/livelive_build/srv6_livelive.json"));

    uint8_t mac_str[6];

    std::ostringstream spreaderMcastSS;
    spreaderMcastSS << "mc_mgrp_create 1\n"
                    << "mc_node_create 1 ";
    for (uint32_t i = 0; i < nPaths; ++i)
    {
        spreaderMcastSS << llFlows + 1 + i << " ";
    }
    spreaderMcastSS << "\nmc_node_associate 1 0" << std::endl;

    std::ostringstream spreaderPortsCommand;
    for (uint32_t i = 0; i < llFlows; i++)
    {
        convertToMacAddress(llSenderIpv6Interfaces[i]->GetDevice()->GetAddress()).CopyTo(mac_str);

        Ipv6Address addr = llSenderIpv6Interfaces[i]->GetAddress(2).GetAddress();
        spreaderPortsCommand << "table_add ipv6_forward forward " << addr << "/128 => " << i + 1
                             << " " << getMacString(mac_str) << std::endl;
    }

    if (testType == "live-live")
    {
        spreaderPortsCommand << spreaderMcastSS.str();
        spreaderPortsCommand
            << "table_add check_live_live_enabled live_live_mcast 2001::/64 => 1 e1::2"
            << std::endl;
        spreaderPortsCommand
            << "table_set_default check_live_live_enabled ipv6_encap_forward_random e1::2 "
            << llFlows + 1 << " " << llFlows + 1 + nPaths << std::endl;
        spreaderPortsCommand << "table_add srv6_live_live_forward add_srv6_ll_segment 1 => e2::55"
                             << std::endl;
        spreaderPortsCommand << "table_add srv6_function srv6_ll_deduplicate 85 => " << std::endl;
    }
    else if (testType == "no-deduplicate")
    {
        spreaderPortsCommand << spreaderMcastSS.str();
        spreaderPortsCommand
            << "table_add check_live_live_enabled live_live_mcast 2001::/64 => 1 e1::2"
            << std::endl;
        spreaderPortsCommand
            << "table_set_default check_live_live_enabled ipv6_encap_forward_random e1::2 "
            << llFlows + 1 << " " << llFlows + 1 + nPaths << std::endl;
        spreaderPortsCommand << "table_add srv6_live_live_forward add_srv6_ll_segment 1 => e2::55"
                             << std::endl;
    }
    else if (testType == "random")
    {
        spreaderPortsCommand
            << "table_add check_live_live_enabled ipv6_encap_forward_random 2001::/64 => e1::2 "
            << llFlows + 1 << " " << llFlows + nPaths << std::endl;

        for (uint32_t i = 0; i < nPaths; ++i)
        {
            spreaderPortsCommand << "table_add srv6_forward add_srv6_dest_segment "
                                 << llFlows + 1 + i << " => e2::2" << std::endl;
        }
    }
    else if (testType == "single")
    {
        spreaderPortsCommand
            << "table_add check_live_live_enabled ipv6_encap_forward_port 2001::/64 => e1::2 "
            << llFlows + 1 << std::endl;

        spreaderPortsCommand << "table_add srv6_forward add_srv6_dest_segment " << llFlows + 1
                             << " => e2::2" << std::endl;
    }

    liveliveHelper.SetDeviceAttribute("PipelineCommands", StringValue(spreaderPortsCommand.str()));
    liveliveHelper.Install(e1, e1Interfaces);

    std::ostringstream despreaderMcastSS;
    despreaderMcastSS << "mc_mgrp_create 1\n"
                      << "mc_node_create 1 ";
    for (uint32_t i = 0; i < nPaths; ++i)
    {
        despreaderMcastSS << 1 + i << " ";
    }
    despreaderMcastSS << "\nmc_node_associate 1 0" << std::endl;

    std::ostringstream despreaderPortsCommand;
    for (uint32_t i = 0; i < llFlows; i++)
    {
        convertToMacAddress(llReceiverIpv6Interfaces[i]->GetDevice()->GetAddress()).CopyTo(mac_str);

        Ipv6Address addr = llReceiverIpv6Interfaces[i]->GetAddress(2).GetAddress();
        despreaderPortsCommand << "table_add ipv6_forward forward " << addr << "/128 => "
                               << 1 + nPaths + i << " " << getMacString(mac_str) << std::endl;
    }

    if (testType == "live-live")
    {
        despreaderPortsCommand << "table_add srv6_function srv6_ll_deduplicate 85 => " << std::endl;
        despreaderPortsCommand << despreaderMcastSS.str();
        despreaderPortsCommand
            << "table_add check_live_live_enabled live_live_mcast 2002::/64 => 1 e2::2"
            << std::endl;
        despreaderPortsCommand
            << "table_set_default check_live_live_enabled ipv6_encap_forward_random e2::2 1 "
            << 1 + nPaths << std::endl;
        despreaderPortsCommand << "table_add srv6_live_live_forward add_srv6_ll_segment 1 => e1::55"
                               << std::endl;
    }
    else if (testType == "no-deduplicate")
    {
        despreaderPortsCommand << despreaderMcastSS.str();
        despreaderPortsCommand
            << "table_add check_live_live_enabled live_live_mcast 2002::/64 => 1 e2::2"
            << std::endl;
        despreaderPortsCommand
            << "table_set_default check_live_live_enabled ipv6_encap_forward_random e2::2 1 "
            << 1 + nPaths << std::endl;
        despreaderPortsCommand << "table_add srv6_live_live_forward add_srv6_ll_segment 1 => e1::55"
                               << std::endl;
    }
    else if (testType == "random")
    {
        despreaderPortsCommand
            << "table_add check_live_live_enabled ipv6_encap_forward_random 2002::/64 => e2::2 1 "
            << 1 + nPaths << std::endl;

        for (uint32_t i = 0; i < nPaths; ++i)
        {
            despreaderPortsCommand << "table_add srv6_forward add_srv6_dest_segment " << 1 + i
                                   << " => e1::2" << std::endl;
        }
    }
    else if (testType == "single")
    {
        despreaderPortsCommand
            << "table_add check_live_live_enabled ipv6_encap_forward_port 2002::/64 => e2::2 1"
            << std::endl;

        despreaderPortsCommand << "table_add srv6_forward add_srv6_dest_segment 1 => e1::2"
                               << std::endl;
    }

    liveliveHelper.SetDeviceAttribute("PipelineCommands",
                                      StringValue(despreaderPortsCommand.str()));
    liveliveHelper.Install(e2, e2Interfaces);

    NS_LOG_INFO("e1 COMMANDS:");
    NS_LOG_INFO(spreaderPortsCommand.str());

    NS_LOG_INFO("e2 COMMANDS:");
    NS_LOG_INFO(despreaderPortsCommand.str());

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

    for (uint32_t i = 0; i < nPaths; ++i)
    {
        NetDeviceContainer nInterfaces = pathsNodesNetDevices[i];
        Ptr<Node> n = forwardSwitches.Get(i);

        forwardHelper.Install(n, nInterfaces);
    }

    std::string rtxPath = getPath(resultsPath, "retransmissions");
    std::filesystem::create_directories(rtxPath);

    NS_LOG_INFO("Create Applications.");
    uint16_t llPort = 40000;
    if (llFlows > 0)
    {
        for (uint32_t i = 0; i < llFlows; i++)
        {
            ApplicationContainer llReceiverApp =
                createSinkTcpApplication(llPort + i, llReceivers.Get(i));
            llReceiverApp.Start(Seconds(0.0));

            Ipv6Address srcAddr = llSenderIpv6Interfaces[i]->GetAddress(2).GetAddress();
            Ipv6Address dstAddr = llReceiverIpv6Interfaces[i]->GetAddress(2).GetAddress();
            ApplicationContainer llSenderApp = createTcpApplication(dstAddr,
                                                                    llPort + i,
                                                                    llSenders.Get(i),
                                                                    llRate,
                                                                    maxBytes,
                                                                    "ns3::" + congestionControl);
            llSenderApp.Start(Seconds(1.0));
        }

        for (uint32_t i = 0; i < llFlows; i++)
        {
            Simulator::Schedule(Seconds(1.1),
                                &startTcpRtx,
                                llReceivers.Get(i)->GetId(),
                                getPath(rtxPath, "ll-rtx.data"));
        }
    }

    std::string tpPath = getPath(resultsPath, "throughput");
    std::filesystem::create_directories(tpPath);
    for (uint32_t i = 0; i < llFlows; i++)
    {
        Simulator::Schedule(Seconds(0),
                            &startThroughputTrace,
                            getPath(tpPath, "ll-" + std::to_string(i) + "-tp.data"),
                            llReceivers.Get(i)->GetId(),
                            0);
    }

    NS_LOG_INFO("Configure Tracing.");
    AsciiTraceHelper ascii;

    std::string cwndPath = getPath(resultsPath, "cwnd");
    std::filesystem::create_directories(cwndPath);

    for (uint32_t i = 0; i < llFlows; i++)
    {
        std::string path = getPath(cwndPath, "ll-sender-" + std::to_string(i) + "-cwnd.data");
        Simulator::Schedule(Seconds(1.1), &TraceCwnd, path, llSenders.Get(i)->GetId());
    }

    if (dumpTraffic)
    {
        std::string tracesPath = getPath(resultsPath, "traces");
        std::filesystem::create_directories(tracesPath);

        csma.EnableAsciiAll(ascii.CreateFileStream(getPath(tracesPath, "p4-switch.tr")));
        csma.EnablePcapAll(getPath(tracesPath, "p4-switch"), true);

        for (uint32_t i = 0; i < nPaths; ++i)
        {
            CsmaHelper pathCsma = pathsCsma[i];
            pathCsma.EnableAsciiAll(ascii.CreateFileStream(getPath(tracesPath, "p4-switch.tr")));
            pathCsma.EnablePcapAll(getPath(tracesPath, "p4-switch"), true);
        }
    }

    FlowMonitorHelper flowHelper;
    Ptr<FlowMonitor> flowMon = flowHelper.Install(NodeContainer(llSenders, llReceivers));

    NS_LOG_INFO("Run Simulation.");
    Simulator::Stop(Seconds(endTime));
    Simulator::Run();
    flowMon->CheckForLostPackets();

    std::string flowMonitorPath = getPath(resultsPath, "flow-monitor");
    std::filesystem::create_directories(flowMonitorPath);
    flowMon->SerializeToXmlFile(getPath(flowMonitorPath, "flow_monitor.xml"), true, true);

    Simulator::Destroy();
    NS_LOG_INFO("Done.");

    /* Delete resources */
    for (auto item : tpStream)
    {
        fclose(item.second);
    }

    for (auto item : rtxStream)
    {
        fclose(item.second);
    }
}
