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


#include "ns3/eff-dsdv-routing-protocol.h"
#include "ns3/test.h"
#include "ns3/mesh-helper.h"
#include "ns3/simulator.h"
#include "ns3/mobility-helper.h"
#include "ns3/double.h"
#include "ns3/uinteger.h"
#include "ns3/v4ping-helper.h"
#include "ns3/string.h"
#include "ns3/boolean.h"
#include "ns3/pcap-file.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/eff-dsdv-packet.h"
#include "ns3/eff-dsdv-rtable.h"


using namespace ns3;
using namespace effdsdv;


/**
 * \ingroup eff-dsdv-test
 * \ingroup tests
 *
 * \brief Type header test case
 */
struct TypeHeaderTest : public TestCase
{
  TypeHeaderTest () : TestCase ("Eff-DSDV TypeHeader")
  {
  }
  virtual void DoRun ()
  {
    TypeHeader h (DSDVTYPE_DSDV);
    NS_TEST_EXPECT_MSG_EQ (h.IsValid (), true, "Default header is valid");
    NS_TEST_EXPECT_MSG_EQ (h.Get (), DSDVTYPE_DSDV, "Default header is DSDV");

    Ptr<Packet> p = Create<Packet> ();
    p->AddHeader (h);
    TypeHeader h2 (DSDVTYPE_RACK);
    uint32_t bytes = p->RemoveHeader (h2);
    NS_TEST_EXPECT_MSG_EQ (bytes, 2, "Type header is 2 byte long");
   // NS_TEST_EXPECT_MSG_EQ (h, h2, "Round trip serialization works"); //?
  }
};

class EffDsdvHeaderTestCase : public TestCase
{
public:
  EffDsdvHeaderTestCase ();
  ~EffDsdvHeaderTestCase ();
  virtual void
  DoRun (void);
};
EffDsdvHeaderTestCase::EffDsdvHeaderTestCase ()
  : TestCase ("Verifying the EffDSDV header")
{
}
EffDsdvHeaderTestCase::~EffDsdvHeaderTestCase ()
{
}
void
EffDsdvHeaderTestCase::DoRun ()
{
  Ptr<Packet> packet = Create<Packet> ();

  {
    effdsdv::DsdvHeader hdr1;
    hdr1.SetDst (Ipv4Address ("10.1.1.2"));
    hdr1.SetDstSeqno (2);
    hdr1.SetHopCount (2);
    packet->AddHeader (hdr1);
    effdsdv::DsdvHeader hdr2;
    hdr2.SetDst (Ipv4Address ("10.1.1.3"));
    hdr2.SetDstSeqno (4);
    hdr2.SetHopCount (1);
    packet->AddHeader (hdr2);
    NS_TEST_ASSERT_MSG_EQ (packet->GetSize (), 20, "001 --> Packet size does not equal 20 bytes"); //WHAT?
  }

  {
    effdsdv::DsdvHeader hdr2;
    packet->RemoveHeader (hdr2);
    NS_TEST_ASSERT_MSG_EQ (hdr2.GetSerializedSize (),10,"002 --> GetSerializedSize does not match 10 bytes");
    NS_TEST_ASSERT_MSG_EQ (hdr2.GetDst (),Ipv4Address ("10.1.1.3"),"003");
    NS_TEST_ASSERT_MSG_EQ (hdr2.GetDstSeqno (),4,"004");
    NS_TEST_ASSERT_MSG_EQ (hdr2.GetHopCount (),1,"005");
    effdsdv::DsdvHeader hdr1;
    uint32_t bytes = packet->RemoveHeader (hdr1);
    NS_TEST_ASSERT_MSG_EQ (hdr1.GetSerializedSize (),10,"006");
    NS_TEST_ASSERT_MSG_EQ (hdr1.GetDst (),Ipv4Address ("10.1.1.2"),"008");
    NS_TEST_ASSERT_MSG_EQ (hdr1.GetDstSeqno (),2,"009");
    NS_TEST_ASSERT_MSG_EQ (hdr1.GetHopCount (),2,"010");
    NS_TEST_ASSERT_MSG_EQ (bytes, 10, "011 --> DSDVHeader does not match 10 bytes");
  }
}

struct RreqHeaderTest : public TestCase
{
  RreqHeaderTest () : TestCase ("Eff-DSDV Rreq")
  {
  }
  virtual void DoRun ()
  {
    effdsdv::RreqHeader h (/*dst*/ Ipv4Address ("1.2.3.4"),/*reserved*/0);
    Ptr<Packet> p = Create<Packet> ();
        p->AddHeader (h);
        RreqHeader h2;
        uint32_t bytes = p->RemoveHeader (h2);
        NS_TEST_EXPECT_MSG_EQ (bytes, 6, "RREP is 6 bytes long");
     //   NS_TEST_EXPECT_MSG_EQ (h, h2, "Round trip serialization works");
  }
};

struct RackHeaderTest : public TestCase
{
  RackHeaderTest () : TestCase ("Eff-DSDV Rack")
  {
  }
  virtual void DoRun ()
  {
    effdsdv::RackHeader h(/*dest*/Ipv4Address ("1.2.3.4"), /*hops*/5,Seconds(10));
    Ptr<Packet> p = Create<Packet> ();
        p->AddHeader (h);
        RackHeader h2;
        uint32_t bytes = p->RemoveHeader (h2);
        NS_TEST_EXPECT_MSG_EQ (bytes, 10, "RREP is 10 bytes long");
       //NS_TEST_EXPECT_MSG_EQ (h, h2, "Round trip serialization works");
  }
};

struct EffDsdvTableTestCase : public TestCase{
	EffDsdvTableTestCase() : TestCase ("Eff-DSDV Table")
	{

	}
	virtual void DoRun()
	{
		effdsdv::RoutingTable rtable;
		  Ptr<NetDevice> dev;
		  {
		    effdsdv::RoutingTableEntry rEntry1 (
		      /*device=*/ dev, /*dst=*/
		      Ipv4Address ("10.1.1.4"), /*seqno=*/ 2,
		      /*iface=*/ Ipv4InterfaceAddress (Ipv4Address ("10.1.1.1"), Ipv4Mask ("255.255.255.0")),
		      /*hops=*/ 2, /*next hop=*/
		      Ipv4Address ("10.1.1.2"),
		      /*lifetime=*/ Seconds (10));
		    NS_TEST_EXPECT_MSG_EQ (rtable.AddRoute (rEntry1),true,"add route");
		    effdsdv::RoutingTableEntry rEntry2 (
		      /*device=*/ dev, /*dst=*/
		      Ipv4Address ("10.1.1.2"), /*seqno=*/ 4,
		      /*iface=*/ Ipv4InterfaceAddress (Ipv4Address ("10.1.1.1"), Ipv4Mask ("255.255.255.0")),
		      /*hops=*/ 1, /*next hop=*/
		      Ipv4Address ("10.1.1.2"),
		      /*lifetime=*/ Seconds (10));
		    NS_TEST_EXPECT_MSG_EQ (rtable.AddRoute (rEntry2),true,"add route");
		    effdsdv::RoutingTableEntry rEntry3 (
		      /*device=*/ dev, /*dst=*/
		      Ipv4Address ("10.1.1.3"), /*seqno=*/ 4,
		      /*iface=*/ Ipv4InterfaceAddress (Ipv4Address ("10.1.1.1"), Ipv4Mask ("255.255.255.0")),
		      /*hops=*/ 1, /*next hop=*/
		      Ipv4Address ("10.1.1.3"),
		      /*lifetime=*/ Seconds (10));
		    NS_TEST_EXPECT_MSG_EQ (rtable.AddRoute (rEntry3),true,"add route");
		    effdsdv::RoutingTableEntry rEntry4 (
		      /*device=*/ dev, /*dst=*/
		      Ipv4Address ("10.1.1.255"), /*seqno=*/ 0,
		      /*iface=*/ Ipv4InterfaceAddress (Ipv4Address ("10.1.1.1"), Ipv4Mask ("255.255.255.0")),
		      /*hops=*/ 0, /*next hop=*/
		      Ipv4Address ("10.1.1.255"),
		      /*lifetime=*/ Seconds (10));
		    NS_TEST_EXPECT_MSG_EQ (rtable.AddRoute (rEntry4),true,"add route");
		    effdsdv::RoutingTableEntry rEntry5 (
		      /*device=*/ dev, /*dst=*/
		      Ipv4Address ("10.1.1.5"), /*seqno=*/ 0,
		      /*iface=*/ Ipv4InterfaceAddress (Ipv4Address ("10.1.1.1"), Ipv4Mask ("255.255.255.0")),
		      /*hops=*/ 1, /*next hop=*/
		      Ipv4Address ("10.1.1.255"),
		      /*lifetime=*/ Seconds (16));
		    NS_TEST_EXPECT_MSG_EQ (rtable.AddRoute (rEntry5),true,"add route");
		  }
		  {
		    effdsdv::RoutingTableEntry rEntry;
		    if (rtable.LookupRoute (Ipv4Address ("10.1.1.4"), rEntry))
		      {
		        NS_TEST_ASSERT_MSG_EQ (rEntry.GetDestination (),Ipv4Address ("10.1.1.4"),"100");
		        NS_TEST_ASSERT_MSG_EQ (rEntry.GetSeqNo (),2,"101");
		        NS_TEST_ASSERT_MSG_EQ (rEntry.GetHop (),2,"102");
		      }
		    if (rtable.LookupRoute (Ipv4Address ("10.1.1.2"), rEntry))
		      {
		        NS_TEST_ASSERT_MSG_EQ (rEntry.GetDestination (),Ipv4Address ("10.1.1.2"),"103");
		        NS_TEST_ASSERT_MSG_EQ (rEntry.GetSeqNo (),4,"104");
		        NS_TEST_ASSERT_MSG_EQ (rEntry.GetHop (),1,"105");
		      }
		    if (rtable.LookupRoute (Ipv4Address ("10.1.1.3"), rEntry))
		      {
		        NS_TEST_ASSERT_MSG_EQ (rEntry.GetDestination (),Ipv4Address ("10.1.1.3"),"106");
		        NS_TEST_ASSERT_MSG_EQ (rEntry.GetSeqNo (),4,"107");
		        NS_TEST_ASSERT_MSG_EQ (rEntry.GetHop (),1,"108");
		      }
		    if (rtable.LookupRoute (Ipv4Address ("10.1.1.255"), rEntry))
		      {
		        NS_TEST_ASSERT_MSG_EQ (rEntry.GetDestination (),Ipv4Address ("10.1.1.255"),"109");
		      }
		    NS_TEST_ASSERT_MSG_EQ (rEntry.GetInterface ().GetLocal (),Ipv4Address ("10.1.1.1"),"110");
		    NS_TEST_ASSERT_MSG_EQ (rEntry.GetInterface ().GetBroadcast (),Ipv4Address ("10.1.1.255"),"111");
		    NS_TEST_ASSERT_MSG_EQ (rtable.RoutingTableSize (),5,"Rtable size incorrect");

		  }
		  Simulator::Destroy ();

	}
};





class EffDsdvTestSuite : public TestSuite
{
public:
  EffDsdvTestSuite ();
};

EffDsdvTestSuite::EffDsdvTestSuite ()
  : TestSuite ("eff-dsdv", UNIT)
{
	//Packet Tests
	  AddTestCase (new EffDsdvHeaderTestCase (), TestCase::QUICK);
	  AddTestCase (new TypeHeaderTest(), TestCase::QUICK);
	  AddTestCase (new RreqHeaderTest(), TestCase::QUICK);
	  AddTestCase (new RackHeaderTest(), TestCase::QUICK);
	//Table Tests
	  AddTestCase (new EffDsdvTableTestCase (), TestCase::QUICK);
}


static EffDsdvTestSuite effDsdvTestSuite;

