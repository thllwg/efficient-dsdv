/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "eff-dsdv-helper.h"
#include "ns3/eff-dsdv-routing-protocol.h"
#include "ns3/node-list.h"
#include "ns3/names.h"
#include "ns3/ipv4-list-routing.h"

namespace ns3 {

EffDsdvHelper::~EffDsdvHelper ()
{
}

EffDsdvHelper::EffDsdvHelper () : Ipv4RoutingHelper ()
{
  m_agentFactory.SetTypeId ("ns3::effdsdv::RoutingProtocol");
}

EffDsdvHelper*
EffDsdvHelper::Copy (void) const
{
  return new EffDsdvHelper (*this);
}

Ptr<Ipv4RoutingProtocol>
EffDsdvHelper::Create (Ptr<Node> node) const
{
  Ptr<effdsdv::RoutingProtocol> agent = m_agentFactory.Create<effdsdv::RoutingProtocol> ();
  node->AggregateObject (agent);
  return agent;
}

void
EffDsdvHelper::Set (std::string name, const AttributeValue &value)
{
  m_agentFactory.Set (name, value);
}




}

