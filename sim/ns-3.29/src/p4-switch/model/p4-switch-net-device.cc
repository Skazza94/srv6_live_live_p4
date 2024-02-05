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
 *
 * Author: Mariano Scazzariello <marianos@kth.se>
 */
#include "p4-switch-net-device.h"

#include "ns3/boolean.h"
#include "ns3/channel.h"
#include "ns3/log.h"
#include "ns3/node.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"
#include "ns3/uinteger.h"
#include "ns3/string.h"
#include "ns3/ethernet-header.h"

/**
 * \file
 * \ingroup p4-switch
 * ns3::P4SwitchNetDevice implementation.
 */

namespace ns3 {
NS_LOG_COMPONENT_DEFINE ("P4SwitchNetDevice");

NS_OBJECT_ENSURE_REGISTERED (P4SwitchNetDevice);

TypeId
P4SwitchNetDevice::GetTypeId ()
{
  static TypeId tid =
      TypeId ("ns3::P4SwitchNetDevice")
          .SetParent<NetDevice> ()
          .SetGroupName ("Switch")
          .AddConstructor<P4SwitchNetDevice> ()
          .AddAttribute (
              "Mtu", "The MAC-level Maximum Transmission Unit", UintegerValue (1500),
              MakeUintegerAccessor (&P4SwitchNetDevice::SetMtu, &P4SwitchNetDevice::GetMtu),
              MakeUintegerChecker<uint16_t> ())
          .AddAttribute ("PipelineJson", "The bmv2 JSON file to use", StringValue (""),
                         MakeStringAccessor (&P4SwitchNetDevice::GetPipelineJson,
                                             &P4SwitchNetDevice::SetPipelineJson),
                         MakeStringChecker ())
          .AddAttribute ("PipelineCommands",
                         "CLI commands to run on the P4 pipeline before starting the simulation",
                         StringValue (""),
                         MakeStringAccessor (&P4SwitchNetDevice::GetPipelineCommands,
                                             &P4SwitchNetDevice::SetPipelineCommands),
                         MakeStringChecker ());

  return tid;
}

P4SwitchNetDevice::P4SwitchNetDevice () : m_node (nullptr), m_ifIndex (0), m_p4_pipeline (nullptr)
{
  NS_LOG_FUNCTION_NOARGS ();
  m_channel = CreateObject<BridgeChannel> ();
}

P4SwitchNetDevice::~P4SwitchNetDevice ()
{
  NS_LOG_FUNCTION_NOARGS ();
  delete m_p4_pipeline;
}

void
P4SwitchNetDevice::DoDispose ()
{
  NS_LOG_FUNCTION_NOARGS ();
  for (std::vector<Ptr<NetDevice>>::iterator iter = m_ports.begin (); iter != m_ports.end ();
       iter++)
    {
      *iter = nullptr;
    }
  m_ports.clear ();
  m_channel = nullptr;
  m_node = nullptr;
  NetDevice::DoDispose ();
}

void
P4SwitchNetDevice::ReceiveFromDevice (Ptr<NetDevice> incomingPort, Ptr<const Packet> packet,
                                      uint16_t protocol, const Address &src, const Address &dst,
                                      PacketType packetType)
{
  NS_LOG_FUNCTION_NOARGS ();
  NS_LOG_DEBUG ("UID is " << packet->GetUid ());

  InitPipeline ();

  NS_LOG_LOGIC ("ReceiveFromDevice sending through P4 pipeline");
  uint32_t port_n = GetPortN (incomingPort);

  std::list<std::pair<uint16_t, Ptr<Packet>>> *pkts = m_p4_pipeline->process (packet, port_n);
  for (auto item : *pkts)
    {
      Ptr<NetDevice> port = GetPort (item.first);
      Ptr<Packet> pkt = item.second;

      EthernetHeader eth_hdr;
      Address pkt_src = GetAddress();
      Address pkt_dst = GetAddress();
      uint32_t bytes_removed = pkt->PeekHeader (eth_hdr);
      if (bytes_removed > 0)
        {
          pkt_src = eth_hdr.GetSource ();
          pkt_dst = eth_hdr.GetDestination ();
        }

      port->SendFrom (item.second, pkt_src, pkt_dst, protocol);
    }

  delete pkts;
}

void
P4SwitchNetDevice::InitPipeline ()
{
  NS_LOG_FUNCTION_NOARGS ();
  if (m_p4_pipeline == nullptr)
    {
      if (m_pipeline_json != "" && !m_pipeline_commands.empty ())
        {
          NS_LOG_DEBUG ("Initializing up P4 pipeline...");
          m_p4_pipeline = new P4Pipeline (m_pipeline_json);
          m_p4_pipeline->run_cli (m_pipeline_commands);
        }
      else
        {
          NS_LOG_ERROR ("Cannot initialize P4 pipeline, abort!");
          std::exit (1);
        }
    }
}

void
P4SwitchNetDevice::AddPort (Ptr<NetDevice> port)
{
  NS_LOG_FUNCTION_NOARGS ();
  NS_ASSERT (port != this);
  if (!Mac48Address::IsMatchingType (port->GetAddress ()))
    {
      NS_FATAL_ERROR ("Device does not support eui 48 addresses: cannot be added to switch.");
    }
  if (!port->SupportsSendFrom ())
    {
      NS_FATAL_ERROR ("Device does not support SendFrom: cannot be added to switch.");
    }
  if (m_address == Mac48Address ())
    {
      m_address = Mac48Address::ConvertFrom (port->GetAddress ());
    }

  NS_LOG_DEBUG ("RegisterProtocolHandler for " << port->GetInstanceTypeId ().GetName ());
  m_node->RegisterProtocolHandler (MakeCallback (&P4SwitchNetDevice::ReceiveFromDevice, this), 0,
                                   port, true);
  m_ports.push_back (port);
  m_channel->AddChannel (port->GetChannel ());
}

uint32_t
P4SwitchNetDevice::GetNPorts () const
{
  NS_LOG_FUNCTION_NOARGS ();
  return m_ports.size ();
}

Ptr<NetDevice>
P4SwitchNetDevice::GetPort (uint32_t n) const
{
  NS_LOG_FUNCTION_NOARGS ();
  return m_ports[n];
}

uint32_t
P4SwitchNetDevice::GetPortN (Ptr<NetDevice> port)
{
  uint32_t n = 0;
  for (std::vector<Ptr<NetDevice>>::iterator iter = m_ports.begin (); iter != m_ports.end ();
       iter++)
    {
      if (port == *iter)
        {
          return n;
        }

      n++;
    }

  return n;
}

std::string
P4SwitchNetDevice::GetPipelineJson () const
{
  NS_LOG_FUNCTION_NOARGS ();
  return m_pipeline_json;
}

void
P4SwitchNetDevice::SetPipelineJson (std::string pipeline_json)
{
  NS_LOG_FUNCTION (this << pipeline_json);
  m_pipeline_json = pipeline_json;
}

std::string
P4SwitchNetDevice::GetPipelineCommands () const
{
  NS_LOG_FUNCTION_NOARGS ();
  return m_pipeline_commands;
}

void
P4SwitchNetDevice::SetPipelineCommands (std::string pipeline_commands)
{
  NS_LOG_FUNCTION (this << pipeline_commands);
  m_pipeline_commands = pipeline_commands;
}

void
P4SwitchNetDevice::SetIfIndex (const uint32_t index)
{
  NS_LOG_FUNCTION_NOARGS ();
  m_ifIndex = index;
}

uint32_t
P4SwitchNetDevice::GetIfIndex () const
{
  NS_LOG_FUNCTION_NOARGS ();
  return m_ifIndex;
}

Ptr<Channel>
P4SwitchNetDevice::GetChannel () const
{
  NS_LOG_FUNCTION_NOARGS ();
  return m_channel;
}

void
P4SwitchNetDevice::SetAddress (Address address)
{
  NS_LOG_FUNCTION_NOARGS ();
  m_address = Mac48Address::ConvertFrom (address);
}

Address
P4SwitchNetDevice::GetAddress () const
{
  NS_LOG_FUNCTION_NOARGS ();
  return m_address;
}

bool
P4SwitchNetDevice::SetMtu (const uint16_t mtu)
{
  NS_LOG_FUNCTION_NOARGS ();
  m_mtu = mtu;
  return true;
}

uint16_t
P4SwitchNetDevice::GetMtu () const
{
  NS_LOG_FUNCTION_NOARGS ();
  return m_mtu;
}

bool
P4SwitchNetDevice::IsLinkUp () const
{
  NS_LOG_FUNCTION_NOARGS ();
  return true;
}

void
P4SwitchNetDevice::AddLinkChangeCallback (Callback<void> callback)
{
  // Unused.
}

bool
P4SwitchNetDevice::IsBroadcast () const
{
  NS_LOG_FUNCTION_NOARGS ();
  return true;
}

Address
P4SwitchNetDevice::GetBroadcast () const
{
  NS_LOG_FUNCTION_NOARGS ();
  return Mac48Address ("ff:ff:ff:ff:ff:ff");
}

bool
P4SwitchNetDevice::IsMulticast () const
{
  NS_LOG_FUNCTION_NOARGS ();
  return true;
}

Address
P4SwitchNetDevice::GetMulticast (Ipv4Address multicastGroup) const
{
  NS_LOG_FUNCTION (this << multicastGroup);
  Mac48Address multicast = Mac48Address::GetMulticast (multicastGroup);
  return multicast;
}

bool
P4SwitchNetDevice::IsPointToPoint () const
{
  NS_LOG_FUNCTION_NOARGS ();
  return false;
}

bool
P4SwitchNetDevice::IsBridge () const
{
  NS_LOG_FUNCTION_NOARGS ();
  return true;
}

bool
P4SwitchNetDevice::Send (Ptr<Packet> packet, const Address &dest, uint16_t protocolNumber)
{
  NS_LOG_FUNCTION_NOARGS ();
  return SendFrom (packet, m_address, dest, protocolNumber);
}

bool
P4SwitchNetDevice::SendFrom (Ptr<Packet> packet, const Address &src, const Address &dest,
                             uint16_t protocolNumber)
{
  NS_LOG_FUNCTION_NOARGS ();
  // Mac48Address dst = Mac48Address::ConvertFrom(dest);

  // InitPipeline();

  // NS_LOG_LOGIC("SendFrom sending through P4 pipeline");
  // std::list<std::pair<uint16_t, Ptr<Packet>>> *pkts = m_p4_pipeline->process(packet);
  // if (pkt->empty())
  //     return false;

  // for (auto item : *pkts)
  // {
  //     Ptr<NetDevice> port = GetPort(item.first);
  //     Mac48Address src48 = Mac48Address::ConvertFrom(src);
  //     Mac48Address dst48 = Mac48Address::ConvertFrom(dest);

  //     port->SendFrom(item.second, src, dst, protocol);
  // }

  // delete pkts;

  // return true;
  return false;
}

Ptr<Node>
P4SwitchNetDevice::GetNode () const
{
  NS_LOG_FUNCTION_NOARGS ();
  return m_node;
}

void
P4SwitchNetDevice::SetNode (Ptr<Node> node)
{
  NS_LOG_FUNCTION_NOARGS ();
  m_node = node;
}

bool
P4SwitchNetDevice::NeedsArp () const
{
  NS_LOG_FUNCTION_NOARGS ();
  return false;
}

void
P4SwitchNetDevice::SetReceiveCallback (NetDevice::ReceiveCallback cb)
{
  NS_LOG_FUNCTION_NOARGS ();
  m_rxCallback = cb;
}

void
P4SwitchNetDevice::SetPromiscReceiveCallback (NetDevice::PromiscReceiveCallback cb)
{
  NS_LOG_FUNCTION_NOARGS ();
  m_promiscRxCallback = cb;
}

bool
P4SwitchNetDevice::SupportsSendFrom () const
{
  NS_LOG_FUNCTION_NOARGS ();
  return false;
}

Address
P4SwitchNetDevice::GetMulticast (Ipv6Address addr) const
{
  NS_LOG_FUNCTION (this << addr);
  return Mac48Address::GetMulticast (addr);
}
} // namespace ns3
