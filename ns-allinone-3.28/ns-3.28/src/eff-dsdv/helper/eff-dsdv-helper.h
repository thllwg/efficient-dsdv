/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#ifndef EFF_DSDV_HELPER_H
#define EFF_DSDV_HELPER_H

#include "ns3/eff-dsdv-routing-protocol.h"
#include "ns3/object-factory.h"
#include "ns3/node.h"
#include "ns3/node-container.h"
#include "ns3/ipv4-routing-helper.h"

namespace ns3 {

/**
 * \ingroup dsdv
 * \brief Helper class that adds DSDV routing to nodes.
 */
class EffDsdvHelper : public Ipv4RoutingHelper
{
public:
  EffDsdvHelper ();
  ~EffDsdvHelper ();
  /**
   * \returns pointer to clone of this DsdvHelper
   *
   * This method is mainly for internal use by the other helpers;
   * clients are expected to free the dynamic memory allocated by this method
   */
  EffDsdvHelper* Copy (void) const;

  /**
   * \param node the node on which the routing protocol will run
   * \returns a newly-created routing protocol
   *
   * This method will be called by ns3::InternetStackHelper::Install
   *
   */
  virtual Ptr<Ipv4RoutingProtocol> Create (Ptr<Node> node) const;
  /**
   * \param name the name of the attribute to set
   * \param value the value of the attribute to set.
   *
   * This method controls the attributes of ns3::dsdv::RoutingProtocol
   */
  void Set (std::string name, const AttributeValue &value);

private:
  ObjectFactory m_agentFactory; //!< Object factory
};



}

#endif /* EFF_DSDV_HELPER_H */

