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
 * Author: Stephen Ibanez <sibanez@stanford.edu>
 * Author: Mariano Scazzariello <marianos@kth.se>
 */

#ifndef P4_PIPELINE_H
#define P4_PIPELINE_H

#define MAX_PKT_SIZE 9000
#define DEFAULT_DROP_PORT 511

#include <bm/bm_sim/packet.h>
#include <bm/bm_sim/switch.h>
#include <bm/bm_sim/simple_pre_lag.h>

#include <memory>
#include <string>
#include <sstream>
#include <list>

#include <ns3/pointer.h>
#include <ns3/packet.h>
#include <ns3/simulator.h>

namespace ns3
{
   /**
    * \ingroup p4-switch
    *
    * A P4 programmable pipeline.
    * This is inspired from ns3-bmv2, with added support for egress processing and multicasting.
    */
   class P4Pipeline : public bm::Switch
   {
   public:
      /**
       * \brief P4Pipeline constructor
       */
      P4Pipeline(std::string jsonFile, std::string name);

      /**
       * \brief Run the provided CLI commands to populate table entries
       */
      void run_cli(std::string commands);

      /**
       * \brief Unused
       */
      int receive_(port_t port_num, const char *buffer, int len) override;

      /**
       * \brief Unused
       */
      void start_and_return_() override;

      /**
       * \brief Invoke the P4 switch processing
       */
      std::list<std::pair<uint16_t, Ptr<Packet>>> *process(Ptr<const Packet> ns3_packet,
                                                           uint32_t ingress_port);

   private:
      enum PktInstanceType
      {
         PKT_INSTANCE_TYPE_NORMAL,
         PKT_INSTANCE_TYPE_INGRESS_CLONE,
         PKT_INSTANCE_TYPE_EGRESS_CLONE,
         PKT_INSTANCE_TYPE_COALESCED,
         PKT_INSTANCE_TYPE_RECIRC,
         PKT_INSTANCE_TYPE_REPLICATION,
         PKT_INSTANCE_TYPE_RESUBMIT,
      };

      /**
       * \brief Invoke the P4 processing ingress pipeline (parser, match-action)
       */
      void process_ingress(std::unique_ptr<bm::Packet> &packet);

      /**
       * \brief Invoke the P4 processing egress pipeline (match-action, deparser)
       */
      void process_egress(std::unique_ptr<bm::Packet> &packet, int pkt_len, uint16_t egress_port);

      /**
       * \brief Process the multicasting of a packet
       */
      void
      process_multicast(std::list<std::pair<uint16_t, std::unique_ptr<bm::Packet>>> *pkts_to_egress,
                        bm::Packet *packet, unsigned int mgid);

      /**
       * \brief Convert the NS3 packet ptr into a bmv2 pkt ptr
       */
      std::unique_ptr<bm::Packet> get_bm_packet(Ptr<const Packet> ns3_packet, uint32_t ingress_port);

      /**
       * \brief Convert the NS3 packet ptr into a bmv2 pkt ptr
       */
      Ptr<Packet> get_ns3_packet(std::unique_ptr<bm::Packet> bm_packet);

   private:
      const uint32_t drop_port = DEFAULT_DROP_PORT;

      static int thrift_port;
      static bm::packet_id_t packet_id;
      static uint8_t ns2bm_buf[MAX_PKT_SIZE];
      std::shared_ptr<bm::McSimplePreLAG> pre;
   };

} // namespace ns3

#endif /* P4_PIPELINE_H */
