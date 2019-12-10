/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
* Copyright (c) 2018 Thorben Ole Hellweg
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
 * Author: Thorben Ole Hellweg <t_hell07@uni-muenster.de>
 *
 * Based on the corresponding DSDV module, provided by Narra et al.
*/

#include "eff-dsdv-routing-protocol.h"
#include "ns3/log.h"
#include "ns3/inet-socket-address.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/wifi-net-device.h"
#include "ns3/boolean.h"
#include "ns3/double.h"
#include "ns3/uinteger.h"
#include "ns3/dsdv-rtable.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("EffDsdvRoutingProtocol");

namespace effdsdv {

NS_OBJECT_ENSURE_REGISTERED (RoutingProtocol);

/// UDP Port for DSDV control traffic
const uint32_t RoutingProtocol::DSDV_PORT = 269;

/// Tag used by DSDV implementation
struct DeferredRouteOutputTag : public Tag
{
  /// Positive if output device is fixed in RouteOutput
  int32_t oif;

  /**
   * Constructor
   *
   * \param o outgoing interface (OIF)
   */
  DeferredRouteOutputTag (int32_t o = -1)
    : Tag (),
      oif (o)
  {
  }

  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId
  GetTypeId ()
  {
    static TypeId tid = TypeId ("ns3::effdsdv::DeferredRouteOutputTag")
      .SetParent<Tag> ()
      .SetGroupName ("EffDsdv")
      .AddConstructor<DeferredRouteOutputTag> ()
    ;
    return tid;
  }

  TypeId
  GetInstanceTypeId () const
  {
    return GetTypeId ();
  }

  uint32_t
  GetSerializedSize () const
  {
    return sizeof(int32_t);
  }

  void
  Serialize (TagBuffer i) const
  {
    i.WriteU32 (oif);
  }

  void
  Deserialize (TagBuffer i)
  {
    oif = i.ReadU32 ();
  }

  void
  Print (std::ostream &os) const
  {
    os << "DeferredRouteOutputTag: output interface = " << oif;
  }
};

TypeId
RoutingProtocol::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::effdsdv::RoutingProtocol")
    .SetParent<Ipv4RoutingProtocol> ()
    .SetGroupName ("EffDsdv")
    .AddConstructor<RoutingProtocol> ()
    .AddAttribute ("PeriodicUpdateInterval","Periodic interval between exchange of full routing tables among nodes. ",
                   TimeValue (Seconds (15)),
                   MakeTimeAccessor (&RoutingProtocol::m_periodicUpdateInterval),
                   MakeTimeChecker ())
    .AddAttribute ("SettlingTime", "Minimum time an update is to be stored in adv table before sending out"
                   "in case of change in metric (in seconds)",
                   TimeValue (Seconds (5)),
                   MakeTimeAccessor (&RoutingProtocol::m_settlingTime),
                   MakeTimeChecker ())
    .AddAttribute ("MaxQueueLen", "Maximum number of packets that we allow a routing protocol to buffer.",
                   UintegerValue (500 /*assuming maximum nodes in simulation is 100*/),
                   MakeUintegerAccessor (&RoutingProtocol::m_maxQueueLen),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("MaxQueuedPacketsPerDst", "Maximum number of packets that we allow per destination to buffer.",
                   UintegerValue (5),
                   MakeUintegerAccessor (&RoutingProtocol::m_maxQueuedPacketsPerDst),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("MaxQueueTime","Maximum time packets can be queued (in seconds)",
                   TimeValue (Seconds (30)),
                   MakeTimeAccessor (&RoutingProtocol::m_maxQueueTime),
                   MakeTimeChecker ())
    .AddAttribute ("EnableBuffering","Enables buffering of data packets if no route to destination is available",
                   BooleanValue (true),
                   MakeBooleanAccessor (&RoutingProtocol::SetEnableBufferFlag,
                                        &RoutingProtocol::GetEnableBufferFlag),
                   MakeBooleanChecker ())
    .AddAttribute ("EnableWST","Enables Weighted Settling Time for the updates before advertising",
                   BooleanValue (true),
                   MakeBooleanAccessor (&RoutingProtocol::SetWSTFlag,
                                        &RoutingProtocol::GetWSTFlag),
                   MakeBooleanChecker ())
    .AddAttribute ("Holdtimes","Times the forwarding Interval to purge the route.",
                   UintegerValue (3),
                   MakeUintegerAccessor (&RoutingProtocol::Holdtimes),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("WeightedFactor","WeightedFactor for the settling time if Weighted Settling Time is enabled",
                   DoubleValue (0.875),
                   MakeDoubleAccessor (&RoutingProtocol::m_weightedFactor),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("EnableRouteAggregation","Enables Weighted Settling Time for the updates before advertising",
                   BooleanValue (false),
                   MakeBooleanAccessor (&RoutingProtocol::SetEnableRAFlag,
                                        &RoutingProtocol::GetEnableRAFlag),
                   MakeBooleanChecker ())
    .AddAttribute ("RouteAggregationTime","Time to aggregate updates before sending them out (in seconds)",
                   TimeValue (Seconds (1)),
                   MakeTimeAccessor (&RoutingProtocol::m_routeAggregationTime),
                   MakeTimeChecker ());
  return tid;
}

void
RoutingProtocol::SetEnableBufferFlag (bool f)
{
  EnableBuffering = f;
}
bool
RoutingProtocol::GetEnableBufferFlag () const
{
  return EnableBuffering;
}
void
RoutingProtocol::SetWSTFlag (bool f)
{
  EnableWST = f;
}
bool
RoutingProtocol::GetWSTFlag () const
{
  return EnableWST;
}
void
RoutingProtocol::SetEnableRAFlag (bool f)
{
  EnableRouteAggregation = f;
}
bool
RoutingProtocol::GetEnableRAFlag () const
{
  return EnableRouteAggregation;
}

int64_t
RoutingProtocol::AssignStreams (int64_t stream)
{
  NS_LOG_FUNCTION (this << stream);
  m_uniformRandomVariable->SetStream (stream);
  return 1;
}

RoutingProtocol::RoutingProtocol ()
  : m_routingTable (),
    m_advRoutingTable (),
    m_queue (),
    m_periodicUpdateTimer (Timer::CANCEL_ON_DESTROY)
{
  m_uniformRandomVariable = CreateObject<UniformRandomVariable> ();
}

RoutingProtocol::~RoutingProtocol ()
{
}

void
RoutingProtocol::DoDispose ()
{
  m_ipv4 = 0;
  for (std::map<Ptr<Socket>, Ipv4InterfaceAddress>::iterator iter = m_socketAddresses.begin (); iter
       != m_socketAddresses.end (); iter++)
    {
      iter->first->Close ();
    }
  m_socketAddresses.clear ();
  Ipv4RoutingProtocol::DoDispose ();
}

void
RoutingProtocol::PrintRoutingTable (Ptr<OutputStreamWrapper> stream, Time::Unit unit) const
{
  *stream->GetStream () << "Node: " << m_mainAddress
                        << ", Time: " << Now ().As (unit)
                        << ", Local time: " << GetObject<Node> ()->GetLocalTime ().As (unit)
                        << ", EffDSDV Routing table" << std::endl;

  m_routingTable.Print (stream);
  *stream->GetStream () << "\n";

	  *stream->GetStream () << "Alternative Table";
		  m_altRoutingTable.Print(stream);


  *stream->GetStream () << std::endl;
}

void
RoutingProtocol::Start ()
{
  m_queue.SetMaxPacketsPerDst (m_maxQueuedPacketsPerDst);
  m_queue.SetMaxQueueLen (m_maxQueueLen);
  m_queue.SetQueueTimeout (m_maxQueueTime);
  m_routingTable.Setholddowntime (Time (Holdtimes * m_periodicUpdateInterval));
  m_advRoutingTable.Setholddowntime (Time (Holdtimes * m_periodicUpdateInterval));
  m_scb = MakeCallback (&RoutingProtocol::Send,this);
  m_ecb = MakeCallback (&RoutingProtocol::Drop,this);
  m_periodicUpdateTimer.SetFunction (&RoutingProtocol::SendPeriodicUpdate,this);
  m_periodicUpdateTimer.Schedule (MicroSeconds (m_uniformRandomVariable->GetInteger (0,1000)));
  //m_periodicUpdateTimer.Schedule (Seconds (m_uniformRandomVariable->GetInteger (0,10)));
}

Ptr<Ipv4Route>
RoutingProtocol::RouteOutput (Ptr<Packet> p,
                              const Ipv4Header &header,
                              Ptr<NetDevice> oif,
                              Socket::SocketErrno &sockerr)
{
  NS_LOG_FUNCTION (this << header << (oif ? oif->GetIfIndex () : 0));

  if (!p)
    {
      return LoopbackRoute (header,oif);
    }
  if (m_socketAddresses.empty ())
    {
      sockerr = Socket::ERROR_NOROUTETOHOST;
      NS_LOG_LOGIC (m_mainAddress << ": No effdsdv interfaces");
      Ptr<Ipv4Route> route;
      return route;
    }
  std::map<Ipv4Address, RoutingTableEntry> removedAddresses;
  std::map<Ipv4Address, RoutingTableEntry> invalidatedAddresses;
  sockerr = Socket::ERROR_NOTERROR;
  Ptr<Ipv4Route> route;
  Ipv4Address dst = header.GetDestination ();
  NS_LOG_DEBUG (m_mainAddress << ": Outgoing -> Packet Size: " << p->GetSize ()
                                << ", Packet id: " << p->GetUid () << ", Destination address in Packet: " << dst);
  m_routingTable.Purge (removedAddresses, invalidatedAddresses);
  if (!removedAddresses.empty ())
    {
	  for (std::map<Ipv4Address, RoutingTableEntry>::iterator rmItr = removedAddresses.begin ();
	       rmItr != removedAddresses.end (); ++rmItr)
	    {
	      rmItr->second.SetEntriesChanged (true);
	      rmItr->second.SetSeqNo (rmItr->second.GetSeqNo () + 1);
	      m_advRoutingTable.AddRoute (rmItr->second);
	    }
      Simulator::Schedule (MicroSeconds (m_uniformRandomVariable->GetInteger (0,1000)),&RoutingProtocol::SendTriggeredUpdate,this);
    }
  if(!invalidatedAddresses.empty())
  {
//	  NS_LOG_DEBUG (m_mainAddress <<": For the recently invalidated...");
//	  for (std::map<Ipv4Address, RoutingTableEntry>::iterator invItr = invalidatedAddresses.begin ();
//	  	       invItr != invalidatedAddresses.end (); ++invItr)
//	  {
//		  NS_LOG_DEBUG (m_mainAddress <<": ..."<<invItr->first<<", subsequent routes using this one get invalidated too");
//		  if (invItr -> second.GetDestination() != invItr -> second.GetNextHop())
//		  {
//			  //InvalidateOverNextHop(invItr -> second.GetNextHop());
//		  }
//	  }

  }
  RoutingTableEntry rt;
  if (LookupRoute(dst,rt))
  //if (m_routingTable.LookupRoute(dst,rt))
    {
      if (EnableBuffering)
        {
          LookForQueuedPackets ();
        }
      if (rt.GetHop () == 1)
        {
          route = rt.GetRoute ();
          NS_ASSERT (route != 0);
          NS_LOG_DEBUG (m_mainAddress << ": A route exists from " << route->GetSource ()
                                               << " to neighboring destination "
                                               << route->GetDestination ());
          if (oif != 0 && route->GetOutputDevice () != oif)
            {
              NS_LOG_DEBUG (m_mainAddress << ": Output device doesn't match. Dropped.");
              sockerr = Socket::ERROR_NOROUTETOHOST;
              return Ptr<Ipv4Route> ();
            }
          return route;
        }
      else
        {
          RoutingTableEntry newrt;
          //if (m_routingTable.LookupRoute (rt.GetNextHop (),newrt))
          if(LookupRoute(rt.GetNextHop(),newrt))
            {
              route = newrt.GetRoute ();
              NS_ASSERT (route != 0);
              NS_LOG_DEBUG (m_mainAddress << ": A route exists from " << route->GetSource ()
                                                   << " to destination " << dst << " via "
                                                   << rt.GetNextHop ());
              if (oif != 0 && route->GetOutputDevice () != oif)
                {
                  NS_LOG_DEBUG (m_mainAddress << ": Output device doesn't match. Dropped.");
                  sockerr = Socket::ERROR_NOROUTETOHOST;
                  return Ptr<Ipv4Route> ();
                }
              return route;
            }
          NS_LOG_DEBUG(m_mainAddress << ": Did not found route");
        }
    }
    else
    {
    	NS_LOG_DEBUG(m_mainAddress<<": Outgoing -> No route to "<<dst<<" found.");
    }

  if (EnableBuffering)
    {
      uint32_t iif = (oif ? m_ipv4->GetInterfaceForDevice (oif) : -1);
      DeferredRouteOutputTag tag (iif);
      if (!p->PeekPacketTag (tag))
        {
          p->AddPacketTag (tag);
        }
    }

  return LoopbackRoute (header,oif);
}

void
RoutingProtocol::DeferredRouteOutput (Ptr<const Packet> p,
                                      const Ipv4Header & header,
                                      UnicastForwardCallback ucb,
                                      ErrorCallback ecb)
{
  NS_LOG_FUNCTION (this << p << header);
  NS_ASSERT (p != 0 && p != Ptr<Packet> ());
  QueueEntry newEntry (p,header,ucb,ecb);
  bool result = m_queue.Enqueue (newEntry);
  if (result)
    {
      NS_LOG_DEBUG (m_mainAddress << ": Added packet " << p->GetUid () << " to queue.");
    }
}

bool
RoutingProtocol::RouteInput (Ptr<const Packet> p,
                             const Ipv4Header &header,
                             Ptr<const NetDevice> idev,
                             UnicastForwardCallback ucb,
                             MulticastForwardCallback mcb,
                             LocalDeliverCallback lcb,
                             ErrorCallback ecb)
{
  NS_LOG_FUNCTION (m_mainAddress << " received packet " << p->GetUid ()
                                 << " from " << header.GetSource ()
                                 << " on interface " << idev->GetAddress ()
                                 << " to destination " << header.GetDestination ());
  if (m_socketAddresses.empty ())
    {
      NS_LOG_DEBUG (m_mainAddress<<": No effdsdv interfaces");
      return false;
    }
  NS_ASSERT (m_ipv4 != 0);
  // Check if input device supports IP
  NS_ASSERT (m_ipv4->GetInterfaceForDevice (idev) >= 0);
  int32_t iif = m_ipv4->GetInterfaceForDevice (idev);

  Ipv4Address dst = header.GetDestination ();
  Ipv4Address origin = header.GetSource ();

  // DSDV is not a multicast routing protocol
  if (dst.IsMulticast ())
    {
      return false;
    }

  // Deferred route request
  if (EnableBuffering == true && idev == m_lo)
    {
      DeferredRouteOutputTag tag;
      if (p->PeekPacketTag (tag))
        {
          DeferredRouteOutput (p,header,ucb,ecb);
          return true;
        }
    }
  for (std::map<Ptr<Socket>, Ipv4InterfaceAddress>::const_iterator j =
         m_socketAddresses.begin (); j != m_socketAddresses.end (); ++j)
    {
      Ipv4InterfaceAddress iface = j->second;
      if (origin == iface.GetLocal ())
        {
          return true;
        }
    }
  // LOCAL DELIVARY TO DSDV INTERFACES
  for (std::map<Ptr<Socket>, Ipv4InterfaceAddress>::const_iterator j = m_socketAddresses.begin (); j
       != m_socketAddresses.end (); ++j)
    {
      Ipv4InterfaceAddress iface = j->second;
      if (m_ipv4->GetInterfaceForAddress (iface.GetLocal ()) == iif)
        {
          if (dst == iface.GetBroadcast () || dst.IsBroadcast ())
            {
              Ptr<Packet> packet = p->Copy ();
              if (lcb.IsNull () == false)
                {
                  NS_LOG_LOGIC (m_mainAddress<<": Broadcast local delivery to " << iface.GetLocal ());
                  lcb (p, header, iif);
                  // Fall through to additional processing
                }
              else
                {
                  NS_LOG_ERROR (m_mainAddress<<": Unable to deliver packet locally due to null callback " << p->GetUid () << " from " << origin);
                  ecb (p, header, Socket::ERROR_NOROUTETOHOST);
                }
              if (header.GetTtl () > 1)
                {
                  NS_LOG_LOGIC (m_mainAddress<<": Forward broadcast. TTL " << (uint16_t) header.GetTtl ());
                  RoutingTableEntry toBroadcast;
                  //if (m_routingTable.LookupRoute (dst,toBroadcast,true)) //TODO:lookup
                  if (LookupRoute(dst,toBroadcast,true))
                    {
                      Ptr<Ipv4Route> route = toBroadcast.GetRoute ();
                      ucb (route,packet,header);
                    }
                  else
                    {
                      NS_LOG_DEBUG (m_mainAddress<<": No route to forward. Drop packet " << p->GetUid ());
                    }
                }
              else
              {
                    NS_LOG_DEBUG (m_mainAddress<<": TTL exceeded. Drop packet " << p->GetUid ());
                }
              return true;
            }
        }
    }

  if (m_ipv4->IsDestinationAddress (dst, iif))
    {
      if (lcb.IsNull () == false)
        {
          NS_LOG_LOGIC ("Unicast local delivery to " << dst);
          lcb (p, header, iif);
        }
      else
        {
          NS_LOG_ERROR (m_mainAddress<<": Unable to deliver packet locally due to null callback " << p->GetUid () << " from " << origin);
          ecb (p, header, Socket::ERROR_NOROUTETOHOST);
        }
      return true;
    }

  // Check if input device supports IP forwarding
  if (m_ipv4->IsForwarding (iif) == false)
    {
      NS_LOG_LOGIC (m_mainAddress<<": Forwarding disabled for this interface");
      ecb (p, header, Socket::ERROR_NOROUTETOHOST);
      return true;
    }

  RoutingTableEntry toDst;
  //if (m_routingTable.LookupRoute (dst,toDst))
  if (LookupRoute(dst,toDst))
    {
      RoutingTableEntry ne;
     // if (m_routingTable.LookupRoute (toDst.GetNextHop (),ne))
      if (LookupRoute (toDst.GetNextHop(),ne))
        {
          Ptr<Ipv4Route> route = ne.GetRoute ();
          NS_LOG_LOGIC (m_mainAddress << ": is forwarding packet " << p->GetUid ()
                                      << " to " << dst
                                      << " from " << header.GetSource ()
                                      << " via nexthop neighbor " << toDst.GetNextHop ());
          ucb (route,p,header);
          return true;
        }
    }
  NS_LOG_LOGIC (m_mainAddress<<": Drop packet " << p->GetUid ()
                               << " as there is no route to forward it.");
  return false;
}

Ptr<Ipv4Route>
RoutingProtocol::LoopbackRoute (const Ipv4Header & hdr, Ptr<NetDevice> oif) const
{
  NS_ASSERT (m_lo != 0);
  Ptr<Ipv4Route> rt = Create<Ipv4Route> ();
  rt->SetDestination (hdr.GetDestination ());
  // rt->SetSource (hdr.GetSource ());
  //
  // Source address selection here is tricky.  The loopback route is
  // returned when DSDV does not have a route; this causes the packet
  // to be looped back and handled (cached) in RouteInput() method
  // while a route is found. However, connection-oriented protocols
  // like TCP need to create an endpoint four-tuple (src, src port,
  // dst, dst port) and create a pseudo-header for checksumming.  So,
  // DSDV needs to guess correctly what the eventual source address
  // will be.
  //
  // For single interface, single address nodes, this is not a problem.
  // When there are possibly multiple outgoing interfaces, the policy
  // implemented here is to pick the first available DSDV interface.
  // If RouteOutput() caller specified an outgoing interface, that
  // further constrains the selection of source address
  //
  std::map<Ptr<Socket>, Ipv4InterfaceAddress>::const_iterator j = m_socketAddresses.begin ();
  if (oif)
    {
      // Iterate to find an address on the oif device
      for (j = m_socketAddresses.begin (); j != m_socketAddresses.end (); ++j)
        {
          Ipv4Address addr = j->second.GetLocal ();
          int32_t interface = m_ipv4->GetInterfaceForAddress (addr);
          if (oif == m_ipv4->GetNetDevice (static_cast<uint32_t> (interface)))
            {
              rt->SetSource (addr);
              break;
            }
        }
    }
  else
    {
      rt->SetSource (j->second.GetLocal ());
    }
  NS_ASSERT_MSG (rt->GetSource () != Ipv4Address (), "Valid EFFDSDV source address not found");
  rt->SetGateway (Ipv4Address ("127.0.0.1"));
  rt->SetOutputDevice (m_lo);
  return rt;
}

void
RoutingProtocol::RecvEffDsdv (Ptr<Socket> socket)
{
	 NS_LOG_FUNCTION (this << socket);
	 Address sourceAddress;
	  Ptr<Packet> advpacket = Create<Packet> ();
	  Ptr<Packet> packet = socket->RecvFrom (sourceAddress);
	  InetSocketAddress inetSourceAddr = InetSocketAddress::ConvertFrom (sourceAddress);
	  Ipv4Address sender = inetSourceAddr.GetIpv4 ();
	  Ipv4Address receiver = m_socketAddresses[socket].GetLocal ();
	  uint32_t packetSize = packet->GetSize ();
	  NS_LOG_FUNCTION (m_mainAddress << ": received Eff-DSDV packet of size: " << packetSize
	                                 << " and packet id: " << packet->GetUid ());
	  NS_LOG_DEBUG (m_mainAddress << ": received Eff-DSDV packet of size: " << packetSize
		                                 << " and packet id: " << packet->GetUid ());
 /*
  NS_LOG_FUNCTION (this << socket);
  Address sourceAddress;
  Ptr<Packet> packet = socket->RecvFrom (sourceAddress);
  InetSocketAddress inetSourceAddr = InetSocketAddress::ConvertFrom (sourceAddress);
  Ipv4Address sender = inetSourceAddr.GetIpv4 ();
  Ipv4Address receiver;

  if (m_socketAddresses.find (socket) != m_socketAddresses.end ())
    {
      receiver = m_socketAddresses[socket].GetLocal ();
    }
  else if (m_socketSubnetBroadcastAddresses.find (socket) != m_socketSubnetBroadcastAddresses.end ())
    {
      receiver = m_socketSubnetBroadcastAddresses[socket].GetLocal ();
    }
  else
    {
      NS_ASSERT_MSG (false, "Received a packet from an unknown socket");
    }
  NS_LOG_DEBUG ("EffDSDV node " << this << " received an EffDSDV packet from " << sender << " to " << receiver);
   */
  bool containedStandardDSDV = false;
  int8_t substractionPacketSize = 0;

  for (; packetSize > 0; packetSize = packetSize - substractionPacketSize)
    {
	  TypeHeader tHeader (DSDVTYPE_DSDV);
	  packet->RemoveHeader (tHeader);
	  if (!tHeader.IsValid ())
	  {
		  NS_LOG_DEBUG (m_mainAddress<< "EffDsdv message " << packet->GetUid () << " with unknown type received: " << tHeader.Get () << ". Drop");
		  return; // drop
	  }
	  switch (tHeader.Get ())
	  {
	  	  case DSDVTYPE_DSDV:
	  	  {
	  		NS_LOG_DEBUG (m_mainAddress<<": Packet "<<packet->GetUid ()<<" contains a DSDV Message");
	  		  containedStandardDSDV = true;
	  		  substractionPacketSize = 12;
	  		  RecvDsdv (packet, receiver, sender);
	  		  break;
		  }
		case DSDVTYPE_RREQ:
		  {
			  NS_LOG_DEBUG (m_mainAddress<<": Packet "<<packet->GetUid ()<<" contains a RREQ Message");
			substractionPacketSize = 8;
		    RecvRouteRequest (packet, receiver, sender);
			break;
		  }
		case DSDVTYPE_RACK:
		  {
			  NS_LOG_DEBUG (m_mainAddress<<": Packet "<<packet->GetUid ()<<" contains a RACK Message");
			  substractionPacketSize = 12;
			  RecvRouteAck (packet, receiver, sender);
			  break;
		  }
		default:
			 NS_LOG_DEBUG (m_mainAddress<<": Unknown package type in EffDSDV based Transmission");
			 break;
	  }
    }
  if (containedStandardDSDV){
  std::map<Ipv4Address, RoutingTableEntry> allRoutes;
  m_advRoutingTable.GetListOfAllValidRoutes (allRoutes);
  if (EnableRouteAggregation && allRoutes.size () > 0)
    {
      Simulator::Schedule (m_routeAggregationTime,&RoutingProtocol::SendTriggeredUpdate,this);
    }
  else
    {
      Simulator::Schedule (MicroSeconds (m_uniformRandomVariable->GetInteger (0,1000)),&RoutingProtocol::SendTriggeredUpdate,this);
    }
  }
}

void
RoutingProtocol::RecvDsdv (Ptr<Packet> p, Ipv4Address receiver, Ipv4Address src)
{
	NS_LOG_FUNCTION (this);

	  uint32_t count = 0;
	  Ptr<NetDevice> dev = m_ipv4->GetNetDevice (m_ipv4->GetInterfaceForAddress (receiver));
	  DsdvHeader dsdvHeader, tempDsdvHeader;
	  p->RemoveHeader (dsdvHeader);
      count = 0;
      NS_LOG_DEBUG (m_mainAddress<<" processes the DSDV packet for " << dsdvHeader.GetDst ());
      /*Verifying if the packets sent by me were returned back to me. If yes, discarding them!*/
      for (std::map<Ptr<Socket>, Ipv4InterfaceAddress>::const_iterator j = m_socketAddresses.begin (); j
           != m_socketAddresses.end (); ++j)
        {
          Ipv4InterfaceAddress interface = j->second;
          if (dsdvHeader.GetDst () == interface.GetLocal ())
            {
              if (dsdvHeader.GetDstSeqno () % 2 == 1)
                {
                  NS_LOG_DEBUG (m_mainAddress<<": Sent effdsdv update back to the same Destination, "
                                "with infinite metric. Time left to send fwd update: "
                                << m_periodicUpdateTimer.GetDelayLeft ());
                  count++;
                }
              else
                {
                  NS_LOG_DEBUG (m_mainAddress<<": Received update for my address. Discarding this.");
                  count++;
                }
            }
        }
      if (count > 0)
      {
    	  return;
      }
      NS_LOG_DEBUG (m_mainAddress<<": Received an effdsdv packet from "
                    << src << ". Details are: Destination: " << dsdvHeader.GetDst () << ", Seq No: "
                    << dsdvHeader.GetDstSeqno () << ", HopCount: " << dsdvHeader.GetHopCount ());
      RoutingTableEntry fwdTableEntry, advTableEntry;
      EventId event;
      bool permanentTableVerifier = m_routingTable.LookupRoute (dsdvHeader.GetDst (),fwdTableEntry);
      if (permanentTableVerifier == false)
        {
          if (dsdvHeader.GetDstSeqno () % 2 != 1)
            {
              NS_LOG_DEBUG (m_mainAddress<<": Received New Route!");
              RoutingTableEntry newEntry (
                /*device=*/ dev, /*dst=*/
                dsdvHeader.GetDst (), /*seqno=*/
                dsdvHeader.GetDstSeqno (),
                /*iface=*/ m_ipv4->GetAddress (m_ipv4->GetInterfaceForAddress (receiver), 0),
                /*hops=*/ dsdvHeader.GetHopCount (), /*next hop=*/
                src, /*lifetime=*/
                Simulator::Now (), /*settlingTime*/
                m_settlingTime, /*entries changed*/
                true);
              newEntry.SetFlag (VALID);
              m_routingTable.AddRoute (newEntry);
              NS_LOG_DEBUG (m_mainAddress<<": New Route added to both tables");
              m_advRoutingTable.AddRoute (newEntry);
            }
          else
            {
              // received update not present in main routing table and also with infinite metric
              NS_LOG_DEBUG (m_mainAddress<<": Discarding this update as this route is not present in "
                            "main routing table and received with infinite metric");
              m_altRoutingTable.DeleteRoute(dsdvHeader.GetDst());
            }
        }
      else
        {
          if (!m_advRoutingTable.LookupRoute (dsdvHeader.GetDst (),advTableEntry))
            {
              RoutingTableEntry tr;
              std::map<Ipv4Address, RoutingTableEntry> allRoutes;
              m_advRoutingTable.GetListOfAllValidRoutes (allRoutes);
              for (std::map<Ipv4Address, RoutingTableEntry>::const_iterator i = allRoutes.begin (); i != allRoutes.end (); ++i)
                {
                  NS_LOG_DEBUG (m_mainAddress<<": ADV table routes are:" << i->second.GetDestination ());
                }
              // present in fwd table and not in advtable
              m_advRoutingTable.AddRoute (fwdTableEntry);
              m_advRoutingTable.LookupRoute (dsdvHeader.GetDst (),advTableEntry);
            }
          if (dsdvHeader.GetDstSeqno () % 2 != 1)
            {
              if (dsdvHeader.GetDstSeqno () > advTableEntry.GetSeqNo ())
                {
                  // Received update with better seq number. Clear any old events that are running
                  if (m_advRoutingTable.ForceDeleteIpv4Event (dsdvHeader.GetDst ()))
                    {
                      NS_LOG_DEBUG (m_mainAddress<<": Canceling the timer to update route with better seq number");
                    }
                  // if its a changed metric *nomatter* where the update came from, wait  for WST
                  if (dsdvHeader.GetHopCount () != advTableEntry.GetHop ())
                    {
                      advTableEntry.SetSeqNo (dsdvHeader.GetDstSeqno ());
                      advTableEntry.SetLifeTime (Simulator::Now ());
                      advTableEntry.SetFlag (VALID);
                      advTableEntry.SetEntriesChanged (true);
                      advTableEntry.SetNextHop (src);
                      advTableEntry.SetHop (dsdvHeader.GetHopCount ());
                      NS_LOG_DEBUG (m_mainAddress<<": Received update with better sequence number and changed metric.Waiting for WST");
                      Time tempSettlingtime = GetSettlingTime (dsdvHeader.GetDst ());
                      advTableEntry.SetSettlingTime (tempSettlingtime);
                      NS_LOG_DEBUG (m_mainAddress<<": Added Settling Time:" << tempSettlingtime.GetSeconds ()
                                                           << "s as there is no event running for this route");
                      event = Simulator::Schedule (tempSettlingtime,&RoutingProtocol::SendTriggeredUpdate,this);
                      m_advRoutingTable.AddIpv4Event (dsdvHeader.GetDst (),event);
                      NS_LOG_DEBUG (m_mainAddress<<": EventCreated EventUID: " << event.GetUid ());
                      // if received changed metric, use it but adv it only after wst
                      m_routingTable.Update (advTableEntry);
                      m_advRoutingTable.Update (advTableEntry);
                    }
                  else
                    {
                      // Received update with better seq number and same metric.
                      advTableEntry.SetSeqNo (dsdvHeader.GetDstSeqno ());
                      advTableEntry.SetLifeTime (Simulator::Now ());
                      advTableEntry.SetFlag (VALID);
                      advTableEntry.SetEntriesChanged (true);
                      advTableEntry.SetNextHop (src);
                      advTableEntry.SetHop (dsdvHeader.GetHopCount ());
                      m_advRoutingTable.Update (advTableEntry);
                      NS_LOG_DEBUG (m_mainAddress<<": Route with better sequence number and same metric received. Advertised without WST");
                    }
                  m_altRoutingTable.DeleteRoute(dsdvHeader.GetDst());
                }
              else if (dsdvHeader.GetDstSeqno () == advTableEntry.GetSeqNo ())
                {
                  if (dsdvHeader.GetHopCount () < advTableEntry.GetHop ())
                    {
                      /*Received update with same seq number and better hop count.
                       * As the metric is changed, we will have to wait for WST before sending out this update.
                       */
                      NS_LOG_DEBUG (m_mainAddress<<": Canceling any existing timer to update route with same sequence number "
                                    "and better hop count");
                      m_advRoutingTable.ForceDeleteIpv4Event (dsdvHeader.GetDst ());
                      advTableEntry.SetSeqNo (dsdvHeader.GetDstSeqno ());
                      advTableEntry.SetLifeTime (Simulator::Now ());
                      advTableEntry.SetFlag (VALID);
                      advTableEntry.SetEntriesChanged (true);
                      advTableEntry.SetNextHop (src);
                      advTableEntry.SetHop (dsdvHeader.GetHopCount ());
                      Time tempSettlingtime = GetSettlingTime (dsdvHeader.GetDst ());
                      advTableEntry.SetSettlingTime (tempSettlingtime);
                      NS_LOG_DEBUG (m_mainAddress<<": Added Settling Time," << tempSettlingtime.GetSeconds ()
                                                           << " as there is no current event running for this route");
                      event = Simulator::Schedule (tempSettlingtime,&RoutingProtocol::SendTriggeredUpdate,this);
                      m_advRoutingTable.AddIpv4Event (dsdvHeader.GetDst (),event);
                      NS_LOG_DEBUG (m_mainAddress<<": EventCreated EventUID: " << event.GetUid ());
                      // if received changed metric, use it but adv it only after wst
                      m_routingTable.Update (advTableEntry);
                      m_advRoutingTable.Update (advTableEntry);
                      m_altRoutingTable.DeleteRoute(advTableEntry.GetDestination());
                    }
                  else
                    {
                      /*Received update with same seq number but with same or greater hop count.
                       * Discard that update.
                       */
                      if (!m_advRoutingTable.AnyRunningEvent (dsdvHeader.GetDst ()))
                        {
                          /*update the timer only if nexthop address matches thus discarding
                           * updates to that destination from other nodes.
                           */
                          if (advTableEntry.GetNextHop () == src)
                            {
                              advTableEntry.SetLifeTime (Simulator::Now ());
                              m_routingTable.Update (advTableEntry);
                            }
                          m_advRoutingTable.DeleteRoute (
                            dsdvHeader.GetDst ());
                        }
                      NS_LOG_DEBUG (m_mainAddress<<": Received update with same seq number and "
                                    "same/worst metric for, " << dsdvHeader.GetDst () << ". Discarding the update.");
                    }
                }
              else
                {
                  // Received update with an old sequence number. Discard the update
                  if (!m_advRoutingTable.AnyRunningEvent (dsdvHeader.GetDst ()))
                    {
                      m_advRoutingTable.DeleteRoute (dsdvHeader.GetDst ());
                    }
                  NS_LOG_DEBUG (dsdvHeader.GetDst () << " : Received update with old seq number. Discarding the update.");
                }
            }
          else
            {
              NS_LOG_DEBUG (m_mainAddress<<": Route with infinite metric received for "
                            << dsdvHeader.GetDst () << " from " << src);
              // Delete route only if update was received from my nexthop neighbor
              if (src == advTableEntry.GetNextHop ())
                {
                  NS_LOG_DEBUG ("Triggering an update for this unreachable route:");
                  std::map<Ipv4Address, RoutingTableEntry> dstsWithNextHopSrc, altDstsWithNextHopSrc;
                  m_routingTable.GetListOfDestinationWithNextHop (dsdvHeader.GetDst (),dstsWithNextHopSrc);
                  m_altRoutingTable.GetListOfDestinationWithNextHop (dsdvHeader.GetDst (),altDstsWithNextHopSrc);
                  m_routingTable.DeleteRoute (dsdvHeader.GetDst ());
                  m_altRoutingTable.DeleteRoute (dsdvHeader.GetDst ());
                  advTableEntry.SetSeqNo (dsdvHeader.GetDstSeqno ());
                  advTableEntry.SetEntriesChanged (true);
                  m_advRoutingTable.Update (advTableEntry);
                  for (std::map<Ipv4Address, RoutingTableEntry>::iterator i = dstsWithNextHopSrc.begin (); i
                       != dstsWithNextHopSrc.end (); ++i)
                    {
                      i->second.SetSeqNo (i->second.GetSeqNo () + 1);
                      i->second.SetEntriesChanged (true);
                      m_advRoutingTable.AddRoute (i->second);
                      m_routingTable.DeleteRoute (i->second.GetDestination ());
                    }
                  for (std::map<Ipv4Address, RoutingTableEntry>::iterator i = altDstsWithNextHopSrc.begin (); i
                                        != altDstsWithNextHopSrc.end (); ++i)
                                     {
                	  	  	  	  	  m_altRoutingTable.DeleteRoute(i->second.GetDestination());
                                     }
                }
              else
                {
                  if (!m_advRoutingTable.AnyRunningEvent (dsdvHeader.GetDst ()))
                    {
                      m_advRoutingTable.DeleteRoute (dsdvHeader.GetDst ());
                    }
                  NS_LOG_DEBUG (dsdvHeader.GetDst () <<
                                " : Discard this link break update as it was received from a different neighbor "
                                "and I can reach the destination");
                }
            }
        }
}


void
RoutingProtocol::RecvRouteRequest (Ptr<Packet> p, Ipv4Address receiver, Ipv4Address src)
{
  NS_LOG_FUNCTION (this);
  RreqHeader rreqHeader;
  p->RemoveHeader (rreqHeader);
  Ipv4Address dst = rreqHeader.GetDst ();

  //check if RREQ can be discarded in favor of next periodic update
  if(m_periodicUpdateTimer.GetDelayLeft().GetSeconds()<1)
  {
	  //discard RREQ
	  NS_LOG_DEBUG (m_mainAddress<<": discard RREQ to "<<dst<<" from "<<src<<" in favor of DSDV-Update");
	  return;
  }
  NS_LOG_DEBUG (receiver << ": received RREQ to destination " << rreqHeader.GetDst ()<<", Packet: "<< p->GetUid() );
  NS_LOG_LOGIC (receiver << ": received RREQ to destination " << rreqHeader.GetDst ()<<", Packet: "<< p->GetUid() );

  std::map<Ipv4Address, RoutingTableEntry> removedAddresses, allRoutes, invalidatedAddresses;
  m_routingTable.Purge (removedAddresses, invalidatedAddresses);

  //  A node generates a RREP if either:
  //  (i)  it is itself the destination,
  //  (ii) or it has an active route to the destination.

  RoutingTableEntry toDst;
  if (m_routingTable.LookupRoute(dst,toDst))
  {
	  if (toDst.GetNextHop () == src)
	  {
		  //Drop RREQ, This node RREP will make a loop.
		  NS_LOG_DEBUG (m_mainAddress<<": Drop RREQ from " << src << ", dest next hop " << toDst.GetNextHop ());
	  }
	  else
	  {
	  RoutingTableEntry nextHop;
	  m_routingTable.LookupRoute(toDst.GetNextHop(),nextHop);
	  if (IsRouteAlive(toDst,0)){
				  //main route is alive
				  if (toDst.GetNextHop()!=toDst.GetDestination())
				  {
					  //next hop is not destination
					 if (IsRouteAlive(nextHop,0)){
						 //use that route
						 NS_LOG_DEBUG (m_mainAddress<<": Found a valid route to "<< toDst.GetDestination());
						 SendRouteAck(toDst,src,receiver);
						 return;
					 }
				  }
				  else
				  {
					  //next hop is already destination, jackpot
					  NS_LOG_DEBUG (m_mainAddress<<": Found a valid route to "<< toDst.GetDestination()<<", which happens to be my neighbour.");
					  SendRouteAck(toDst,src,receiver);
					  return;
				  }
			  }
	  }
  }
  else if (m_altRoutingTable.LookupRoute(dst,toDst))
  {
	  if (toDst.GetNextHop () == src)
	  {
	  	 //Drop RREQ, This node RREP will make a loop.
		 NS_LOG_DEBUG (m_mainAddress<<": Drop RREQ from " << src << ", dest next hop " << toDst.GetNextHop ());
		 //AND Delete alternative Route, as our route towards destination is not functional anymore
		 m_altRoutingTable.DeleteRoute(dst);
	  	 return;
	  }
	  RoutingTableEntry nextHop;
	  m_altRoutingTable.LookupRoute(toDst.GetNextHop(),nextHop);
	  if (toDst.GetInstallTime().GetSeconds()<=(Seconds(5)) && toDst.GetFlag()==RouteFlags::VALID){
		  //alt route is alive
		  if (toDst.GetNextHop()!=toDst.GetDestination())
	  	  {
			  //next hop is not destination
	  		  if (nextHop.GetInstallTime().GetSeconds()<=(Seconds(5)) && nextHop.GetFlag()==RouteFlags::VALID)
	  		  {
	  			//use that route
	  			NS_LOG_DEBUG (m_mainAddress<<": Found a valid alternative route to "<< toDst.GetDestination());
	  			SendRouteAck(toDst,src,receiver);
	  			return;
	  		  }
	  	  }
		  else
		  {
			  //next hop is already destination, jackpot
			  NS_LOG_DEBUG (m_mainAddress<<": Found a valid alternative route to "<< toDst.GetDestination()<<", which happens to be my neighbour.");
			  SendRouteAck(toDst,src,receiver);
			  return;
		  }
	  }
  }
  else
  {
  	NS_LOG_DEBUG (m_mainAddress<<": Drop RREQ as no valid entry for "<< dst <<" in main or in alternative routing table");
  }
}

void
RoutingProtocol::RecvRouteAck (Ptr<Packet> p, Ipv4Address receiver, Ipv4Address sender)
{
  NS_LOG_FUNCTION (this << " src " << sender);
  RackHeader rackHeader;
  p->RemoveHeader (rackHeader);
  Ipv4Address dst = rackHeader.GetDst ();
  NS_LOG_LOGIC (receiver <<": received RACK for destination " << dst << " from " << sender);
  NS_LOG_DEBUG (receiver <<": received RACK for destination " << dst << " from " << sender);
  //check if main route has already been reestablished
  RoutingTableEntry mainRte;
  if(m_routingTable.LookupRoute(dst,mainRte))
  {
//	  if(mainRte.GetFlag()==RouteFlags::VALID){
//		  NS_LOG_DEBUG(m_mainAddress<<" discards RACK for " << dst << " as valid Route has been found in main Table");
//		  return;
//	  }
	  if(mainRte.GetNextHop()==sender && (int(mainRte.GetLifeTime().GetSeconds())-int(rackHeader.GetUpdateTime().GetSeconds())>Seconds(1)))
	  {
		  NS_LOG_DEBUG(m_mainAddress<<" discards RACK for " << dst << " as that knowledge is already in main table");
		  return;
	  }
  }
  uint8_t hop = rackHeader.GetHopCount ();
  rackHeader.SetHopCount (hop);
  Ptr<NetDevice> dev = m_ipv4->GetNetDevice (m_ipv4->GetInterfaceForAddress (receiver));
  RoutingTableEntry newEntry (
 	                  /*device=*/ dev, /*dst=*/
 	                  dst, /*seqno=*/ 0,
 	                  /*iface=*/ m_ipv4->GetAddress (m_ipv4->GetInterfaceForAddress (receiver), 0),
 	                  /*hops=*/ hop, /*next hop=*/
 	                  sender, /*lifetime=*/
 					  Simulator::Now()-Time(rackHeader.GetUpdateTime()), /*settlingTime*/
 	                  m_settlingTime, /*entries changed*/
 	                  true);
 	                newEntry.SetFlag (VALID);
 	                newEntry.SetInstallTime(Simulator::Now ());
 RoutingTableEntry rt;
  if (m_altRoutingTable.LookupRoute(dst,rt))
  {
	  if (rt.GetFlag()==RouteFlags::INSEARCH)
	  {
		  m_altRoutingTable.Update(newEntry);
		  NS_LOG_DEBUG (m_mainAddress<<": Valid alternative to "<<dst<<" saved to Routing Table");
	  } else
	  {
		  if (hop <= rt.GetHop())
		  {
			  if (hop == rt.GetHop())
			  {
				  if (rackHeader.GetUpdateTime().GetSeconds()<rt.GetLifeTime())
				  {
					 rt.SetNextHop(sender);
					 rt.SetLifeTime(Simulator::Now()-Time(rackHeader.GetUpdateTime()));
					 rt.SetInstallTime(Simulator::Now ());
					 m_altRoutingTable.Update(rt);
					 NS_LOG_DEBUG (m_mainAddress<<": more recent update for "<<dst<<" saved");
				  }
			  } else
			  {
				  rt.SetNextHop(sender);
				  rt.SetLifeTime(Simulator::Now()-Time(rackHeader.GetUpdateTime()));
				  rt.SetHop(hop);
				  rt.SetInstallTime(Simulator::Now ());
				  m_altRoutingTable.Update(rt);
				  NS_LOG_DEBUG (m_mainAddress<<": shorter alternative route to "<<dst<<" saved");
			  }
		  }
	  }
  } else {
	  m_altRoutingTable.AddRoute(newEntry);
		  NS_LOG_DEBUG (m_mainAddress<<": Alternative Route to "<<dst<<" saved to Routing Table. No preliminary entry found to replace.");
  }
}



void
RoutingProtocol::SendRouteRequest (Ipv4Address dst)
{
  NS_LOG_FUNCTION ( this << dst);
  // Create RREQ header
  RreqHeader rreqHeader;
  rreqHeader.SetDst (dst);
  RoutingTableEntry rt;
  /**
   * Check if Request is already in progress
   * If lifetime of request is expired, delete
   * else discard request
   */
  if (m_altRoutingTable.LookupRoute(dst,rt))
  {
	  if (rt.GetFlag()==RouteFlags::INSEARCH && rt.GetLifeTime()<=Seconds(4)){
		  //discard
		 NS_LOG_DEBUG(m_mainAddress <<": RREQ still in progress, discard new request");
		  return;
	  }
	  else if (rt.GetFlag()==RouteFlags::INSEARCH && rt.GetLifeTime()>Seconds(4))
	  {
		  NS_LOG_DEBUG(m_mainAddress << ": RREQ not answered in time, send new request");
		  m_altRoutingTable.DeleteRoute(dst);
	  }
  }
  if(!m_altRoutingTable.LookupRoute(dst,rt))
  {
	  Ptr<NetDevice> dev = m_ipv4->GetNetDevice (m_ipv4->GetInterfaceForAddress (m_mainAddress));
	   RoutingTableEntry newEntry (
	   	                  /*device=*/ dev, /*dst=*/
	   	                  dst, /*seqno=*/ 0,
	   	                  /*iface=*/ m_ipv4->GetAddress (m_ipv4->GetInterfaceForAddress (m_mainAddress), 0),
	   	                  /*hops=*/ 0, /*next hop=*/
	 					  m_mainAddress, /*lifetime=*/
	 					  Simulator::Now (), /*settlingTime*/
	   	                  m_settlingTime, /*entries changed*/
	   	                  true);
	   	                newEntry.SetFlag (RouteFlags::INSEARCH);
	   	  m_altRoutingTable.AddRoute(newEntry);
	   	NS_LOG_DEBUG(m_mainAddress <<": Placeholder set up in alternative table for "<<dst);
  }


  // Send RREQ as subnet directed broadcast from each interface used by effdsdv
	    for (std::map<Ptr<Socket>, Ipv4InterfaceAddress>::const_iterator j =
	           m_socketAddresses.begin (); j != m_socketAddresses.end (); ++j)
	      {
	        Ptr<Socket> socket = j->first;
	        Ipv4InterfaceAddress iface = j->second;

	        Ptr<Packet> packet = Create<Packet> ();
	        packet->AddHeader (rreqHeader);
	        TypeHeader tHeader (DSDVTYPE_RREQ);
	        packet->AddHeader (tHeader);
	        // Send to all-hosts broadcast if on /32 addr, subnet-directed otherwise
	        Ipv4Address destination;
	        if (iface.GetMask () == Ipv4Mask::GetOnes ())
	          {
	            destination = Ipv4Address ("255.255.255.255");
	          }
	        else
	          {
	            destination = iface.GetBroadcast ();
	          }
	        NS_LOG_DEBUG (m_mainAddress<<": Send RREQ for dst " << rreqHeader.GetDst() << " to socket");
	        Simulator::Schedule (Time (MilliSeconds (m_uniformRandomVariable->GetInteger (0, 50))), &RoutingProtocol::SendTo, this, socket, packet, destination);
	        //SendTo(socket,packet,destination);
	      }
}

void
RoutingProtocol::SendRouteAck (RoutingTableEntry const & toDst, Ipv4Address requester, Ipv4Address acknowledger)
{
  NS_LOG_FUNCTION (this << acknowledger);
  RackHeader rackHeader ( /*dst=*/ toDst.GetDestination(), /*hopCount=*/ toDst.GetHop()+1, /*updateTime=*/ toDst.GetLifeTime());
  Ptr<Packet> packet = Create<Packet> ();
  packet->AddHeader (rackHeader);
  TypeHeader tHeader (DSDVTYPE_RACK);
  packet->AddHeader (tHeader);
  Ptr<Socket> socket = FindSocketWithInterfaceAddress (m_ipv4->GetAddress (m_ipv4->GetInterfaceForAddress (m_mainAddress), 0));
  NS_ASSERT (socket);
  NS_LOG_DEBUG(m_mainAddress<<": Send RACK to " <<requester<< " from "<<acknowledger<<" for destination "<<toDst.GetDestination() << ", costs "<< toDst.GetHop()+1);
  NS_LOG_DEBUG(m_mainAddress<<": via Socket: "<<socket);
  socket->SendTo (packet, 0, InetSocketAddress (requester, DSDV_PORT));
  //Simulator::Schedule (Time (MilliSeconds (m_uniformRandomVariable->GetInteger (0, 10))), &RoutingProtocol::SendTo, this, socket, packet, requester);
}

void
RoutingProtocol::SendTriggeredUpdate ()
{
  NS_LOG_FUNCTION (m_mainAddress << " is sending a triggered update");
  std::map<Ipv4Address, RoutingTableEntry> allRoutes;
  m_advRoutingTable.GetListOfAllValidRoutes (allRoutes);
  for (std::map<Ptr<Socket>, Ipv4InterfaceAddress>::const_iterator j = m_socketAddresses.begin (); j
       != m_socketAddresses.end (); ++j)
    {
      DsdvHeader dsdvHeader;
      Ptr<Socket> socket = j->first;
      Ipv4InterfaceAddress iface = j->second;
      Ptr<Packet> packet = Create<Packet> ();
      for (std::map<Ipv4Address, RoutingTableEntry>::const_iterator i = allRoutes.begin (); i != allRoutes.end (); ++i)
        {
          NS_LOG_LOGIC (m_mainAddress<<": Destination: " << i->second.GetDestination ()
                                        << " SeqNo:" << i->second.GetSeqNo () << " HopCount:"
                                        << i->second.GetHop () + 1);
          RoutingTableEntry temp = i->second;
          if ((i->second.GetEntriesChanged () == true) && (!m_advRoutingTable.AnyRunningEvent (temp.GetDestination ())))
            {
              dsdvHeader.SetDst (i->second.GetDestination ());
              dsdvHeader.SetDstSeqno (i->second.GetSeqNo ());
              dsdvHeader.SetHopCount (i->second.GetHop () + 1);
              temp.SetFlag (VALID);
              temp.SetEntriesChanged (false);
              m_advRoutingTable.DeleteIpv4Event (temp.GetDestination ());
              if (!(temp.GetSeqNo () % 2))
                {
                  m_routingTable.Update (temp);
                }
              packet->AddHeader (dsdvHeader);
              TypeHeader tHeader (DSDVTYPE_DSDV);
              packet->AddHeader (tHeader);
              m_advRoutingTable.DeleteRoute (temp.GetDestination ());
              NS_LOG_DEBUG (m_mainAddress<<": Deleted this route from the advertised table");
            }
          else
            {
              EventId event = m_advRoutingTable.GetEventId (temp.GetDestination ());
              NS_ASSERT (event.GetUid () != 0);
              NS_LOG_DEBUG (m_mainAddress<<": EventID " << event.GetUid () << " associated with "
                                       << temp.GetDestination () << " has not expired, waiting in adv table");
            }
        }
      if (packet->GetSize () >= 12)
        {
          RoutingTableEntry temp2;
          m_routingTable.LookupRoute (m_ipv4->GetAddress (1, 0).GetBroadcast (), temp2);
          dsdvHeader.SetDst (m_ipv4->GetAddress (1, 0).GetLocal ());
          dsdvHeader.SetDstSeqno (temp2.GetSeqNo ());
          dsdvHeader.SetHopCount (temp2.GetHop () + 1);
          NS_LOG_DEBUG (m_mainAddress<<": Adding my update as well to the packet");
          packet->AddHeader (dsdvHeader);
          TypeHeader tHeader (DSDVTYPE_DSDV);
          packet->AddHeader (tHeader);
          // Send to all-hosts broadcast if on /32 addr, subnet-directed otherwise
          Ipv4Address destination;
          if (iface.GetMask () == Ipv4Mask::GetOnes ())
            {
              destination = Ipv4Address ("255.255.255.255");
            }
          else
            {
              destination = iface.GetBroadcast ();
            }
          socket->SendTo (packet, 0, InetSocketAddress (destination, DSDV_PORT));
          NS_LOG_FUNCTION (m_mainAddress<<": Sent Triggered Update from "
                           << dsdvHeader.GetDst ()
                           << " with packet id : " << packet->GetUid () << " and packet Size: " << packet->GetSize ());
        }
      else
        {
          NS_LOG_FUNCTION (m_mainAddress<<": Update not sent as there are no updates to be triggered");
        }
    }
}

void
RoutingProtocol::SendPeriodicUpdate ()
{
  std::map<Ipv4Address, RoutingTableEntry> removedAddresses, allRoutes, invalidatedAddresses;
  m_routingTable.Purge (removedAddresses, invalidatedAddresses);
  MergeTriggerPeriodicUpdates ();
  m_routingTable.GetListOfAllRoutes (allRoutes);
  if (allRoutes.empty ())
    {
      return;
    }
  NS_LOG_FUNCTION (m_mainAddress << " is sending out its periodic update");
  for (std::map<Ptr<Socket>, Ipv4InterfaceAddress>::const_iterator j = m_socketAddresses.begin (); j
       != m_socketAddresses.end (); ++j)
    {
      Ptr<Socket> socket = j->first;
      Ipv4InterfaceAddress iface = j->second;
      Ptr<Packet> packet = Create<Packet> ();
      for (std::map<Ipv4Address, RoutingTableEntry>::const_iterator i = allRoutes.begin (); i != allRoutes.end (); ++i)
        {
          DsdvHeader dsdvHeader;
          if (i->second.GetHop () == 0)
            {
              RoutingTableEntry ownEntry;
              dsdvHeader.SetDst (m_ipv4->GetAddress (1,0).GetLocal ());
              dsdvHeader.SetDstSeqno (i->second.GetSeqNo () + 2);
              dsdvHeader.SetHopCount (i->second.GetHop () + 1);
              m_routingTable.LookupRoute (m_ipv4->GetAddress (1,0).GetBroadcast (),ownEntry);
              ownEntry.SetSeqNo (dsdvHeader.GetDstSeqno ());
              m_routingTable.Update (ownEntry);
              packet->AddHeader (dsdvHeader);
              TypeHeader tHeader (DSDVTYPE_DSDV);
              packet->AddHeader (tHeader);
            }
          else
            {
              dsdvHeader.SetDst (i->second.GetDestination ());
              dsdvHeader.SetDstSeqno ((i->second.GetSeqNo ()));
              dsdvHeader.SetHopCount (i->second.GetHop () + 1);
              packet->AddHeader (dsdvHeader);
              TypeHeader tHeader (DSDVTYPE_DSDV);
              packet->AddHeader (tHeader);
            }
          NS_LOG_DEBUG (m_mainAddress<<": Forwarding the update for " << i->first);
          NS_LOG_DEBUG (m_mainAddress<<": Forwarding details are, Destination: " << dsdvHeader.GetDst ()
                                                                << ", SeqNo:" << dsdvHeader.GetDstSeqno ()
                                                                << ", HopCount:" << dsdvHeader.GetHopCount ()
                                                                << ", LifeTime: " << i->second.GetLifeTime ().GetSeconds ());
        }
      for (std::map<Ipv4Address, RoutingTableEntry>::const_iterator rmItr = removedAddresses.begin (); rmItr
           != removedAddresses.end (); ++rmItr)
        {
          DsdvHeader removedHeader;
          removedHeader.SetDst (rmItr->second.GetDestination ());
          removedHeader.SetDstSeqno (rmItr->second.GetSeqNo () + 1);
          removedHeader.SetHopCount (rmItr->second.GetHop () + 1);
          packet->AddHeader (removedHeader);
          TypeHeader tHeader (DSDVTYPE_DSDV);
          packet->AddHeader (tHeader);
          NS_LOG_DEBUG (m_mainAddress<<": Update for removed record is: Destination: " << removedHeader.GetDst ()
                                                                      << " SeqNo:" << removedHeader.GetDstSeqno ()
                                                                      << " HopCount:" << removedHeader.GetHopCount ());
        }
      socket->Send (packet);
      // Send to all-hosts broadcast if on /32 addr, subnet-directed otherwise
      Ipv4Address destination;
      if (iface.GetMask () == Ipv4Mask::GetOnes ())
        {
          destination = Ipv4Address ("255.255.255.255");
        }
      else
        {
          destination = iface.GetBroadcast ();
        }
      socket->SendTo (packet, 0, InetSocketAddress (destination, DSDV_PORT));
      NS_LOG_FUNCTION (m_mainAddress<<": PeriodicUpdate Packet UID is : " << packet->GetUid ());
    }
  m_periodicUpdateTimer.Schedule (m_periodicUpdateInterval + MicroSeconds (25 * m_uniformRandomVariable->GetInteger (0,1000)));
}

void
RoutingProtocol::SetIpv4 (Ptr<Ipv4> ipv4)
{
  NS_ASSERT (ipv4 != 0);
  NS_ASSERT (m_ipv4 == 0);
  m_ipv4 = ipv4;
  // Create lo route. It is asserted that the only one interface up for now is loopback
  NS_ASSERT (m_ipv4->GetNInterfaces () == 1 && m_ipv4->GetAddress (0, 0).GetLocal () == Ipv4Address ("127.0.0.1"));
  m_lo = m_ipv4->GetNetDevice (0);
  NS_ASSERT (m_lo != 0);
  // Remember lo route
  RoutingTableEntry rt (
    /*device=*/ m_lo,  /*dst=*/
    Ipv4Address::GetLoopback (), /*seqno=*/
    0,
    /*iface=*/ Ipv4InterfaceAddress (Ipv4Address::GetLoopback (),Ipv4Mask ("255.0.0.0")),
    /*hops=*/ 0,  /*next hop=*/
    Ipv4Address::GetLoopback (),
    /*lifetime=*/ Simulator::GetMaximumSimulationTime ());
  rt.SetFlag (INVALID);
  rt.SetEntriesChanged (false);
  m_routingTable.AddRoute (rt);
  Simulator::ScheduleNow (&RoutingProtocol::Start,this);
}

void
RoutingProtocol::NotifyInterfaceUp (uint32_t i)
{
  NS_LOG_FUNCTION (this << m_ipv4->GetAddress (i, 0).GetLocal ()
                        << " interface is up");
  Ptr<Ipv4L3Protocol> l3 = m_ipv4->GetObject<Ipv4L3Protocol> ();
  Ipv4InterfaceAddress iface = l3->GetAddress (i,0);
  if (iface.GetLocal () == Ipv4Address ("127.0.0.1"))
    {
      return;
    }
  // Create a socket to listen only on this interface
  Ptr<Socket> socket = Socket::CreateSocket (GetObject<Node> (),UdpSocketFactory::GetTypeId ());
  NS_ASSERT (socket != 0);
  socket->SetRecvCallback (MakeCallback (&RoutingProtocol::RecvEffDsdv,this));
  socket->BindToNetDevice (l3->GetNetDevice (i));
  socket->Bind (InetSocketAddress (Ipv4Address::GetAny (), DSDV_PORT));
  socket->SetAllowBroadcast (true);
  socket->SetAttribute ("IpTtl",UintegerValue (1));
  m_socketAddresses.insert (std::make_pair (socket,iface));
  // Add local broadcast record to the routing table
  Ptr<NetDevice> dev = m_ipv4->GetNetDevice (m_ipv4->GetInterfaceForAddress (iface.GetLocal ()));
  RoutingTableEntry rt (/*device=*/ dev, /*dst=*/ iface.GetBroadcast (), /*seqno=*/ 0,/*iface=*/ iface,/*hops=*/ 0,
                                    /*next hop=*/ iface.GetBroadcast (), /*lifetime=*/ Simulator::GetMaximumSimulationTime ());
  m_routingTable.AddRoute (rt);
  if (m_mainAddress == Ipv4Address ())
    {
      m_mainAddress = iface.GetLocal ();
    }
  NS_ASSERT (m_mainAddress != Ipv4Address ());
}

void
RoutingProtocol::NotifyInterfaceDown (uint32_t i)
{
  Ptr<Ipv4L3Protocol> l3 = m_ipv4->GetObject<Ipv4L3Protocol> ();
  Ptr<NetDevice> dev = l3->GetNetDevice (i);
  Ptr<Socket> socket = FindSocketWithInterfaceAddress (m_ipv4->GetAddress (i,0));
  NS_ASSERT (socket);
  socket->Close ();
  m_socketAddresses.erase (socket);
  if (m_socketAddresses.empty ())
    {
      NS_LOG_LOGIC ("No effdsdv interfaces");
      m_routingTable.Clear ();
      return;
    }
  m_routingTable.DeleteAllRoutesFromInterface (m_ipv4->GetAddress (i,0));
  m_advRoutingTable.DeleteAllRoutesFromInterface (m_ipv4->GetAddress (i,0));
}

void
RoutingProtocol::NotifyAddAddress (uint32_t i,
                                   Ipv4InterfaceAddress address)
{
  NS_LOG_FUNCTION (this << " interface " << i << " address " << address);
  Ptr<Ipv4L3Protocol> l3 = m_ipv4->GetObject<Ipv4L3Protocol> ();
  if (!l3->IsUp (i))
    {
      return;
    }
  Ipv4InterfaceAddress iface = l3->GetAddress (i,0);
  Ptr<Socket> socket = FindSocketWithInterfaceAddress (iface);
  if (!socket)
    {
      if (iface.GetLocal () == Ipv4Address ("127.0.0.1"))
        {
          return;
        }
      Ptr<Socket> socket = Socket::CreateSocket (GetObject<Node> (),UdpSocketFactory::GetTypeId ());
      NS_ASSERT (socket != 0);
      socket->SetRecvCallback (MakeCallback (&RoutingProtocol::RecvEffDsdv,this));
      // Bind to any IP address so that broadcasts can be received
      socket->BindToNetDevice (l3->GetNetDevice (i));
      socket->Bind (InetSocketAddress (Ipv4Address::GetAny (), DSDV_PORT));
      socket->SetAllowBroadcast (true);
      m_socketAddresses.insert (std::make_pair (socket,iface));
      Ptr<NetDevice> dev = m_ipv4->GetNetDevice (m_ipv4->GetInterfaceForAddress (iface.GetLocal ()));
      RoutingTableEntry rt (/*device=*/ dev, /*dst=*/ iface.GetBroadcast (),/*seqno=*/ 0, /*iface=*/ iface,/*hops=*/ 0,
                                        /*next hop=*/ iface.GetBroadcast (), /*lifetime=*/ Simulator::GetMaximumSimulationTime ());
      m_routingTable.AddRoute (rt);
    }
}

void
RoutingProtocol::NotifyRemoveAddress (uint32_t i,
                                      Ipv4InterfaceAddress address)
{
  Ptr<Socket> socket = FindSocketWithInterfaceAddress (address);
  if (socket)
    {
      m_socketAddresses.erase (socket);
      Ptr<Ipv4L3Protocol> l3 = m_ipv4->GetObject<Ipv4L3Protocol> ();
      if (l3->GetNAddresses (i))
        {
          Ipv4InterfaceAddress iface = l3->GetAddress (i,0);
          // Create a socket to listen only on this interface
          Ptr<Socket> socket = Socket::CreateSocket (GetObject<Node> (),UdpSocketFactory::GetTypeId ());
          NS_ASSERT (socket != 0);
          socket->SetRecvCallback (MakeCallback (&RoutingProtocol::RecvEffDsdv,this));
          // Bind to any IP address so that broadcasts can be received
          socket->Bind (InetSocketAddress (Ipv4Address::GetAny (), DSDV_PORT));
          socket->SetAllowBroadcast (true);
          m_socketAddresses.insert (std::make_pair (socket,iface));
        }
    }
}

Ptr<Socket>
RoutingProtocol::FindSocketWithInterfaceAddress (Ipv4InterfaceAddress addr) const
{
  for (std::map<Ptr<Socket>, Ipv4InterfaceAddress>::const_iterator j = m_socketAddresses.begin (); j
       != m_socketAddresses.end (); ++j)
    {
      Ptr<Socket> socket = j->first;
      Ipv4InterfaceAddress iface = j->second;
      if (iface == addr)
        {
          return socket;
        }
    }
  Ptr<Socket> socket;
  return socket;
}

void
RoutingProtocol::Send (Ptr<Ipv4Route> route,
                       Ptr<const Packet> packet,
                       const Ipv4Header & header)
{
  Ptr<Ipv4L3Protocol> l3 = m_ipv4->GetObject<Ipv4L3Protocol> ();
  NS_ASSERT (l3 != 0);
  Ptr<Packet> p = packet->Copy ();
  l3->Send (p,route->GetSource (),header.GetDestination (),header.GetProtocol (),route);
}

void
RoutingProtocol::SendTo (Ptr<Socket> socket, Ptr<Packet> packet, Ipv4Address destination)
{
  socket->SendTo (packet, 0, InetSocketAddress (destination, DSDV_PORT));

}

void
RoutingProtocol::Drop (Ptr<const Packet> packet,
                       const Ipv4Header & header,
                       Socket::SocketErrno err)
{
  NS_LOG_DEBUG (m_mainAddress << " drop packet " << packet->GetUid () << " to "
                              << header.GetDestination () << " from queue. Error " << err);
}

void
RoutingProtocol::LookForQueuedPackets ()
{
  NS_LOG_FUNCTION (this);
  Ptr<Ipv4Route> route;
  std::map<Ipv4Address, RoutingTableEntry> allRoutes;
  GetListOfAllRoutes (allRoutes);
  //m_routingTable.GetListOfAllValidRoutes(allRoutes);
  for (std::map<Ipv4Address, RoutingTableEntry>::const_iterator i = allRoutes.begin (); i != allRoutes.end (); ++i)
    {
      RoutingTableEntry rt;
      rt = i->second;
      if (m_queue.Find (rt.GetDestination ()))
        {
          if (rt.GetHop () == 1)
            {
              route = rt.GetRoute ();
              NS_LOG_LOGIC (m_mainAddress<<": A route exists from " << route->GetSource ()
                                                   << " to neighboring destination "
                                                   << route->GetDestination ());
              NS_LOG_DEBUG (m_mainAddress<<": A route exists from " << route->GetSource ()
                                                                 << " to neighboring destination "
                                                                 << route->GetDestination ());
              NS_ASSERT (route != 0);
            }
          else
            {
              RoutingTableEntry newrt;
              //m_routingTable.LookupRoute (rt.GetNextHop (),newrt);
              if(LookupRoute(rt.GetNextHop (), newrt))
              {
            	  route = newrt.GetRoute ();
            	                NS_LOG_LOGIC (m_mainAddress<<": A route exists from " << route->GetSource ()
            	                                                                   << " to destination " << route->GetDestination () << " via "
            	                                                                   << rt.GetNextHop ());
            	                NS_LOG_DEBUG (m_mainAddress<<": A route exists from " << route->GetSource ()
            	                                                     << " to destination " << route->GetDestination () << " via "
            	                                                     << rt.GetNextHop ());
            	                NS_ASSERT (route != 0);
              } else {
            	  continue;
              }
            }
          SendPacketFromQueue (rt.GetDestination (),route);
        }
    }
}

void
RoutingProtocol::SendPacketFromQueue (Ipv4Address dst,
                                      Ptr<Ipv4Route> route)
{
  NS_LOG_DEBUG (m_mainAddress << " is sending a queued packet to destination " << dst);
  QueueEntry queueEntry;
  if (m_queue.Dequeue (dst,queueEntry))
    {
      DeferredRouteOutputTag tag;
      Ptr<Packet> p = ConstCast<Packet> (queueEntry.GetPacket ());
      if (p->RemovePacketTag (tag))
        {
          if (tag.oif != -1 && tag.oif != m_ipv4->GetInterfaceForDevice (route->GetOutputDevice ()))
            {
              NS_LOG_DEBUG (m_mainAddress<<": Output device doesn't match. Dropped.");
              return;
            }
        }
      UnicastForwardCallback ucb = queueEntry.GetUnicastForwardCallback ();
      Ipv4Header header = queueEntry.GetIpv4Header ();
      header.SetSource (route->GetSource ());
      header.SetTtl (header.GetTtl () + 1); // compensate extra TTL decrement by fake loopback routing
      ucb (route,p,header);
      if (m_queue.GetSize () != 0 && m_queue.Find (dst))
        {
          Simulator::Schedule (MilliSeconds (m_uniformRandomVariable->GetInteger (0,100)),
                               &RoutingProtocol::SendPacketFromQueue,this,dst,route);
        }
    }
}

Time
RoutingProtocol::GetSettlingTime (Ipv4Address address)
{
  NS_LOG_FUNCTION (m_mainAddress<<": Calculating the settling time for " << address);
  RoutingTableEntry mainrt;
  Time weightedTime;
  m_routingTable.LookupRoute (address,mainrt);
  if (EnableWST)
    {
      if (mainrt.GetSettlingTime () == Seconds (0))
        {
          return Seconds (0);
        }
      else
        {
          NS_LOG_DEBUG (m_mainAddress<<": Route SettlingTime: " << mainrt.GetSettlingTime ().GetSeconds ()
                                               << " and LifeTime:" << mainrt.GetLifeTime ().GetSeconds ());
          weightedTime = Time (m_weightedFactor * mainrt.GetSettlingTime ().GetSeconds () + (1.0 - m_weightedFactor)
                               * mainrt.GetLifeTime ().GetSeconds ());
          NS_LOG_DEBUG ("Calculated weightedTime:" << weightedTime.GetSeconds ());
          return weightedTime;
        }
    }
  return mainrt.GetSettlingTime ();
}

void
RoutingProtocol::MergeTriggerPeriodicUpdates ()
{
  NS_LOG_FUNCTION (m_mainAddress<<": Merging advertised table changes with main table before sending out periodic update");
  std::map<Ipv4Address, RoutingTableEntry> allRoutes;
  m_advRoutingTable.GetListOfAllValidRoutes (allRoutes);
  if (allRoutes.size () > 0)
    {
      for (std::map<Ipv4Address, RoutingTableEntry>::const_iterator i = allRoutes.begin (); i != allRoutes.end (); ++i)
        {
          RoutingTableEntry advEntry = i->second;
          if ((advEntry.GetEntriesChanged () == true) && (!m_advRoutingTable.AnyRunningEvent (advEntry.GetDestination ())))
            {
              if (!(advEntry.GetSeqNo () % 2))
                {
                  advEntry.SetFlag (VALID);
                  advEntry.SetEntriesChanged (false);
                  m_routingTable.Update (advEntry);
                  NS_LOG_DEBUG (m_mainAddress<<": Merged update for " << advEntry.GetDestination () << " with main routing Table");
                }
              m_advRoutingTable.DeleteRoute (advEntry.GetDestination ());
            }
          else
            {
              NS_LOG_DEBUG ("Event currently running. Cannot Merge Routing Tables");
            }
        }
    }
}
bool
RoutingProtocol::IsMyOwnAddress (Ipv4Address src)
{
  NS_LOG_FUNCTION (this << src);
  for (std::map<Ptr<Socket>, Ipv4InterfaceAddress>::const_iterator j =
         m_socketAddresses.begin (); j != m_socketAddresses.end (); ++j)
    {
      Ipv4InterfaceAddress iface = j->second;
      if (src == iface.GetLocal ())
        {
          return true;
        }
    }
  return false;
}

void
RoutingProtocol::InvalidateOverNextHop (Ipv4Address nextHop)
{
	std::map<Ipv4Address, RoutingTableEntry> dstsWithNextHopSrc, altDstsWithNextHopSrc;
	m_routingTable.GetListOfDestinationWithNextHop (nextHop,dstsWithNextHopSrc);
	for (std::map<Ipv4Address, RoutingTableEntry>::iterator k = dstsWithNextHopSrc.begin (); k
						  != dstsWithNextHopSrc.end (); ++k)
	{
		RoutingTableEntry t = k->second;
		NS_LOG_DEBUG (m_mainAddress<<": A route ("<<k->second.GetDestination()<<") using "<<nextHop<<" as next hop has also been invalidated");
		t.SetFlag(RouteFlags::INVALID);
		m_routingTable.Update(t);
	}
	m_altRoutingTable.GetListOfDestinationWithNextHop (nextHop,altDstsWithNextHopSrc);
	for (std::map<Ipv4Address, RoutingTableEntry>::iterator k = altDstsWithNextHopSrc.begin (); k
						  != altDstsWithNextHopSrc.end (); ++k)
	{
		NS_LOG_DEBUG (m_mainAddress<<": Subsequently, matching alternative routes have been deleted:"<< k->second.GetDestination());
		m_altRoutingTable.DeleteRoute(k->second.GetDestination());
	}
}

bool
RoutingProtocol::IsRouteAlive(RoutingTableEntry rt, double bufferInSec)
{
	if (rt.GetLifeTime()>(m_periodicUpdateInterval+Seconds(bufferInSec)))
	{
		if (rt.GetFlag()==RouteFlags::VALID){
		}
		return false;
	} else if (rt.GetFlag()==RouteFlags::VALID)
	{
		return true;
	}
	else
	{
		return false;
	}
}

bool
RoutingProtocol::LookupRoute (Ipv4Address id,
                           RoutingTableEntry & rt)
{
	RoutingTableEntry altRt;
	if (m_routingTable.LookupRoute(id,rt))
	  {
		  //there is a main route
		  RoutingTableEntry nextHop;
		  m_routingTable.LookupRoute(rt.GetNextHop(),nextHop);
		  if (IsRouteAlive(rt,2)){
			  //main route is alive
			  if (rt.GetNextHop()!=rt.GetDestination())
			  {
				  //next hop is not destination
				 if (IsRouteAlive(nextHop,2)){
					 //use that route
					 return true;
				 }
			  }
			  else
			  {
				  return true;
			  }
		  }
		  else if (rt.GetNextHop()!=rt.GetDestination() && IsRouteAlive(nextHop,2)){
			  //main route is not alive
			  //but the next section of it is
			  //so we're not a the point of link breakage
			  return true;
		  }
		  else if (m_altRoutingTable.LookupRoute(id,altRt))
		  {
			  //we discovered a point of link breakage
			  NS_LOG_DEBUG(m_mainAddress<<": Couldn't find valid main route to "<<id<<", starting alternative lookup");
			  if (altRt.GetFlag()==RouteFlags::INSEARCH)
			  {
				  //already looking for an alternative route
				  NS_LOG_DEBUG(m_mainAddress<<": Ongoing Route Request to "<<id<<", no valid route available at this time");
				  SendRouteRequest(id);
				  return false;
			  }
			  else if (altRt.GetFlag()==RouteFlags::VALID)
			  {
				  //return the alternative route
				  NS_LOG_DEBUG (m_mainAddress<<": Found an alternative Route to "<< id <<" via "<<altRt.GetNextHop()<<" instead of "<<rt.GetNextHop());
				  rt.SetSettlingTime(altRt.GetSettlingTime());
				  rt.SetFlag(altRt.GetFlag());
				  rt.SetHop(altRt.GetHop());
				  rt.SetNextHop(altRt.GetNextHop());
				  rt.SetSeqNo(altRt.GetSeqNo());
				  rt.SetLifeTime(altRt.GetLifeTime());
				  rt.SetInterface(altRt.GetInterface());
				  rt.SetOutputDevice(altRt.GetOutputDevice());
				  rt.SetRoute(altRt.GetRoute());

				  if (altRt.GetInstallTime().GetSeconds()>(m_periodicUpdateInterval.GetSeconds()/3))
				  {
					  NS_LOG_DEBUG(m_mainAddress<<": Alternative route to "<<id<<" installed at "<<altRt.GetInstallTime().GetSeconds()<<" sec. ago, requesting more recent information...");
					  SendRouteRequest(id);
				  }
				  if (altRt.GetInstallTime().GetSeconds()>(m_periodicUpdateInterval.GetSeconds()/3)+2)
				  {
					  m_altRoutingTable.DeleteRoute(altRt.GetDestination());
					  return false;
				  }
				  return true;
			  }
		  }
		  else
		  {
			  NS_LOG_DEBUG(m_mainAddress<<": Couldn't find any valid route to "<<id<<", sending out request");
			  SendRouteRequest(id);
			  return false;
		  }
	  }
	  NS_LOG_DEBUG(m_mainAddress<<": Destination node "<<id<<" unknown");
	  return false;
}

bool
RoutingProtocol::LookupRoute (Ipv4Address id,
                           RoutingTableEntry & rt,
						   bool forRouteInput)
{
	RoutingTableEntry altRt;
		NS_LOG_DEBUG (m_mainAddress<<": Searching both routing tables for "<<id);
		if (m_routingTable.LookupRoute(id,rt,forRouteInput))
		  {
			  //there is a main route
			  RoutingTableEntry nextHop;
			  m_routingTable.LookupRoute(rt.GetNextHop(),nextHop);
			  if (IsRouteAlive(rt,2)){
				  //main route is alive
				  if (rt.GetNextHop()!=rt.GetDestination())
				  {
					  //next hop is not destination
					 if (IsRouteAlive(nextHop,2)){
						 //use that route
						 return true;
					 }
				  }
				  else
				  			  {
				  				  return true;
				  			  }
			  }
			  else if (rt.GetNextHop()!=rt.GetDestination() && IsRouteAlive(nextHop,2)){
				  //main route is not alive
				  //but the next section of it is
				  //so we're not a the point of link breakage
				  return true;
			  }
			  else if (m_altRoutingTable.LookupRoute(id,altRt,forRouteInput))
			  {
				  //we discovered a point of link breakage
				  NS_LOG_DEBUG(m_mainAddress<<": Couldn't find valid main route to "<<id<<", starting alternative lookup");
				  if (altRt.GetFlag()==RouteFlags::INSEARCH)
				  {
					  //already looking for an alternative route
					  NS_LOG_DEBUG(m_mainAddress<<": Ongoing Route Request to "<<id<<", no valid route available at this time");
					  SendRouteRequest(id);
					  return false;
				  }
				  else if (altRt.GetFlag()==RouteFlags::VALID)
				  {
					  //return the alternative route
					  NS_LOG_DEBUG (m_mainAddress<<": Found an alternative Route to "<< id <<" via "<<altRt.GetNextHop()<<" instead of "<<rt.GetNextHop());
					  rt.SetSettlingTime(altRt.GetSettlingTime());
					  				  rt.SetFlag(altRt.GetFlag());
					  				  rt.SetHop(altRt.GetHop());
					  				  rt.SetNextHop(altRt.GetNextHop());
					  				  rt.SetSeqNo(altRt.GetSeqNo());
					  				  rt.SetLifeTime(altRt.GetLifeTime());
					  				  rt.SetInterface(altRt.GetInterface());
					  				  rt.SetOutputDevice(altRt.GetOutputDevice());
					  				  rt.SetRoute(altRt.GetRoute());
					  if (altRt.GetInstallTime().GetSeconds()>(m_periodicUpdateInterval.GetSeconds()/3))
					  {
						  NS_LOG_DEBUG(m_mainAddress<<": Alternative route to "<<id<<" installed at "<<altRt.GetInstallTime().GetSeconds()<<" sec. ago, requesting more recent information...");
						  SendRouteRequest(id);
					  }
					  if (altRt.GetInstallTime().GetSeconds()>(m_periodicUpdateInterval.GetSeconds()/3)+2)
					  {
						  m_altRoutingTable.DeleteRoute(altRt.GetDestination());
						  return false;
					  }
					  return true;
				  }
			  }
			  else
			  {
				  NS_LOG_DEBUG(m_mainAddress<<": Couldn't find any valid route to "<<id<<", sending out request");
				  SendRouteRequest(id);
				  return false;
			  }
		  }
		  NS_LOG_DEBUG(m_mainAddress<<": Destination node "<<id<<" unknown");
		  return false;

//	RoutingTableEntry altRt;
//  if (m_routingTable.LookupRoute(id,rt,forRouteInput)){
//	  //clean invalid routes
//	  if (rt.GetHop()>0)
//	  {
//		  RoutingTableEntry nhEntry;
//		  if (m_routingTable.LookupRoute(rt.GetNextHop(),nhEntry,forRouteInput))
//		  {
//			  if (nhEntry.GetLifeTime()>(m_periodicUpdateInterval+Seconds(2)) && nhEntry.GetFlag()==RouteFlags::VALID)
//			  {
//				  NS_LOG_DEBUG(m_mainAddress<<": On Lookup, the route to neighboring "<<nhEntry.GetDestination()<<" has been found without recent updates. Invalidate now...");
//				  nhEntry.SetFlag(RouteFlags::INVALID);
//				  m_routingTable.Update(nhEntry);
//				  NS_LOG_DEBUG (m_mainAddress<<": Invalidated neighboring "<<nhEntry.GetDestination());
//				  std::map<Ipv4Address, RoutingTableEntry> dstsWithNextHopSrc, altDstsWithNextHopSrc;
//				  m_routingTable.GetListOfDestinationWithNextHop (nhEntry.GetDestination(),dstsWithNextHopSrc);
//				  for (std::map<Ipv4Address, RoutingTableEntry>::iterator k = dstsWithNextHopSrc.begin (); k
//					  != dstsWithNextHopSrc.end (); ++k)
//				  {
//					  RoutingTableEntry t = k->second;
//					  NS_LOG_DEBUG (m_mainAddress<<": A route ("<<k->second.GetDestination()<<") using this neighbour as a next hop has also been invalidated");
//					  t.SetFlag(RouteFlags::INVALID);
//					  m_routingTable.Update(t);
//				  }
//				  m_altRoutingTable.GetListOfDestinationWithNextHop (nhEntry.GetDestination(),altDstsWithNextHopSrc);
//				  for (std::map<Ipv4Address, RoutingTableEntry>::iterator k = altDstsWithNextHopSrc.begin (); k
//					  != altDstsWithNextHopSrc.end (); ++k)
//				  {
//					  NS_LOG_DEBUG (m_mainAddress<<": Subsequently, matching alternative routes have been deleted:"<< k->second.GetDestination());
//					  m_altRoutingTable.DeleteRoute(k->second.GetDestination());
//				  }
//
//			  }
//		  }
//	  }
//	  m_routingTable.LookupRoute(id,rt,forRouteInput);
//	  if (rt.GetFlag()==RouteFlags::VALID)
//	  {
//		  return true;
//	  }
//	  else if (m_altRoutingTable.LookupRoute(id,altRt,forRouteInput))
//	  {
//		  NS_LOG_DEBUG(m_mainAddress<<": Couldn't find valid main route to "<<id<<", starting alternative lookup");
//		  if (altRt.GetFlag()==RouteFlags::INSEARCH)
//		  {
//			  NS_LOG_DEBUG(m_mainAddress<<": Ongoing Route Request to "<<id<<", no valid route available at this time");
//			  SendRouteRequest(id);
//			  return false;
//		  }
//		  else
//		  {
//			  //return the alternative route
//			  NS_LOG_DEBUG (m_mainAddress<<": Found an alternative Route to "<< id <<" via "<<altRt.GetNextHop()<<" instead of "<<rt.GetNextHop());
//			  rt = altRt;
//			  if (altRt.GetInstallTime().GetSeconds()>(m_periodicUpdateInterval.GetSeconds()/3))
//			  {
//			  	  NS_LOG_DEBUG(m_mainAddress<<": Alternative route to "<<id<<" installed at "<<altRt.GetInstallTime().GetSeconds()<<" sec. ago, requesting more recent information...");
//			  	  SendRouteRequest(id);
//			  }
//			  if (altRt.GetInstallTime().GetSeconds()>(m_periodicUpdateInterval.GetSeconds()/3)+2)
//			  {
//				  m_altRoutingTable.DeleteRoute(altRt.GetDestination());
//				  return false;
//			  }
//			  return true;
//		  }
//	  }
//	  else
//	  {
//		  NS_LOG_DEBUG(m_mainAddress<<": Couldn't find any valid route to "<<id<<", sending out request");
//		  SendRouteRequest(id);
//		  return false;
//		  //return true;
//	  }
//  }
//  NS_LOG_DEBUG(m_mainAddress<<": Destination node unknown");
//  return false;
	//#########################
//	RoutingTableEntry altRt;
//	if (m_routingTable.LookupRoute(id,rt,forRouteInput)){
//		if (rt.GetHop()==1)
//		{
//				  if (rt.GetLifeTime()>(m_periodicUpdateInterval+Seconds(2)) && rt.GetFlag()==RouteFlags::VALID)
//				  {
//					  NS_LOG_DEBUG(m_mainAddress<<": On Lookup, the route to "<<id<<" has been found without recent updates. Invalidate now...");
//					  rt.SetFlag(RouteFlags::INVALID);
//				  }
//				  if (rt.GetFlag()==RouteFlags::VALID){
//
//					  return true;
//				  }
//				  else if(m_altRoutingTable.LookupRoute(id,altRt,forRouteInput))
//				  {
//					  NS_LOG_DEBUG(m_mainAddress<<": Couldn't find valid main route to "<<id<<", starting alternative lookup");
//					  if (altRt.GetFlag()==RouteFlags::INSEARCH)
//					  {
//						  NS_LOG_DEBUG(m_mainAddress<<": Ongoing Route Request to "<<id<<", no valid route available at this time");
//						  SendRouteRequest(id);
//						  return false;
//						  //return true;
//					  } else {
//						  //return the alternative route
//						  NS_LOG_DEBUG (m_mainAddress<<": Found an alternative Route to "<< id <<" via "<<altRt.GetNextHop()<<" instead of "<<rt.GetNextHop());
//						  rt = altRt;
//						  if (altRt.GetInstallTime().GetSeconds()>(m_periodicUpdateInterval.GetSeconds()/3)){
//							  NS_LOG_DEBUG(m_mainAddress<<": Alternative route to "<<id<<" installed at "<<altRt.GetInstallTime().GetSeconds()<<" sec. ago, requesting more recent information...");
//							  SendRouteRequest(id);
//						  }
//						  if (altRt.GetInstallTime().GetSeconds()>(m_periodicUpdateInterval.GetSeconds()/3)+2){
//							  m_altRoutingTable.DeleteRoute(altRt.GetDestination());
//							  return false;
//						  }
//						  return true;
//					  }
//				  } else
//				  {
//					  NS_LOG_DEBUG(m_mainAddress<<": Couldn't find any valid route to "<<id<<", sending out request");
//					  SendRouteRequest(id);
//					  return false;
//					  //return true;
//				  }
//		}
//		else
//		{
//			return true;
//		}
//	} else
//	{
//		return false;
//	}
// ################################
//	RoutingTableEntry altRt;
//  if (m_routingTable.LookupRoute(id,rt,forRouteInput)){
//	  if (rt.GetLifeTime()>(m_periodicUpdateInterval+Seconds(2)) && rt.GetFlag()==RouteFlags::VALID)
//	  {
//		  NS_LOG_DEBUG(m_mainAddress<<": On Lookup, the route to "<<id<<" has been found without recent updates. Invalidate now...");
//		  rt.SetFlag(RouteFlags::INVALID);
//	  }
//	  if (rt.GetFlag()==RouteFlags::VALID){
//
//		  return true;
//	  }
//	  else if(m_altRoutingTable.LookupRoute(id,altRt,forRouteInput))
//	  {
//		  NS_LOG_DEBUG(m_mainAddress<<": Couldn't find valid main route to "<<id<<", starting alternative lookup");
//		  if (altRt.GetFlag()==RouteFlags::INSEARCH)
//		  {
//			  NS_LOG_DEBUG(m_mainAddress<<": Ongoing Route Request to "<<id<<", no valid route available at this time");
//			  SendRouteRequest(id);
//			  return false;
//			  //return true;
//		  } else {
//			  //return the alternative route
//			  NS_LOG_DEBUG (m_mainAddress<<": Found an alternative Route to "<< id <<" via "<<altRt.GetNextHop()<<" instead of "<<rt.GetNextHop());
//			  rt = altRt;
//			  if (altRt.GetInstallTime().GetSeconds()>(m_periodicUpdateInterval.GetSeconds()/3)){
//				  NS_LOG_DEBUG(m_mainAddress<<": Alternative route to "<<id<<" installed at "<<altRt.GetInstallTime().GetSeconds()<<" sec. ago, requesting more recent information...");
//				  SendRouteRequest(id);
//			  }
//			  if (altRt.GetInstallTime().GetSeconds()>(m_periodicUpdateInterval.GetSeconds()/3)+2){
//			 				  m_altRoutingTable.DeleteRoute(altRt.GetDestination());
//			 				  return false;
//			 			  }
//			  return true;
//		  }
//	  } else
//	  {
//		  NS_LOG_DEBUG(m_mainAddress<<": Couldn't find any valid route to "<<id<<", sending out request");
//		  SendRouteRequest(id);
//		  return false;
//		  //return true;
//	  }
//  }
//  return false;
}

void
RoutingProtocol::GetListOfAllRoutes (std::map<Ipv4Address, RoutingTableEntry> & allRoutes)
{
	std::map<Ipv4Address, RoutingTableEntry>  routeCollection;
	m_routingTable.GetListOfAllRoutes(routeCollection);
	if (!routeCollection.empty())
	{
		for (std::map<Ipv4Address, RoutingTableEntry>::const_iterator i = routeCollection.begin (); i != routeCollection.end (); ++i)
		{
			RoutingTableEntry rte;
			rte = i->second;
			if (rte.GetFlag()==RouteFlags::INVALID){
				RoutingTableEntry altRte;
				if (m_altRoutingTable.LookupRoute(rte.GetDestination(),altRte))
				{
					if (altRte.GetFlag()==RouteFlags::VALID)
					{
//						rte.SetFlag(altRte.GetFlag());
//						rte.SetHop(altRte.GetHop());
//						rte.SetNextHop(altRte.GetNextHop());
//						rte.SetSeqNo(altRte.GetSeqNo());
//						rte.SetLifeTime(altRte.GetLifeTime());
//						rte.SetInterface(altRte.GetInterface());
//						rte.SetOutputDevice(altRte.GetOutputDevice());
//						rte.SetRoute(altRte.GetRoute());
						allRoutes.insert(std::make_pair(altRte.GetDestination(),altRte));
					}
				}
			}
			else
			{
				allRoutes.insert(std::make_pair(i->first,i->second));
			}
		}
	}
	else
	{
		NS_LOG_DEBUG (m_mainAddress<<": No entries in main Routing Table");
	}
}

}
}

