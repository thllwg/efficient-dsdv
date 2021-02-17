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
#include "eff-dsdv-packet.h"
#include "ns3/address-utils.h"
#include "ns3/packet.h"

namespace ns3 {
namespace effdsdv {

NS_OBJECT_ENSURE_REGISTERED (TypeHeader);

TypeHeader::TypeHeader (MessageType t)
  : m_type (t),
    m_valid (true)
{
}

TypeId
TypeHeader::GetTypeId ()
{
  static TypeId tid = TypeId ("ns3::effdsdv::TypeHeader")
    .SetParent<Header> ()
    .SetGroupName ("EffDsdv")
    .AddConstructor<TypeHeader> ()
  ;
  return tid;
}

TypeId
TypeHeader::GetInstanceTypeId () const
{
  return GetTypeId ();
}

uint32_t
TypeHeader::GetSerializedSize () const
{
  return 2;
}

void
TypeHeader::Serialize (Buffer::Iterator i) const
{
  i.WriteU16 ((uint16_t) m_type);
}

uint32_t
TypeHeader::Deserialize (Buffer::Iterator start)
{
  Buffer::Iterator i = start;
  uint16_t type = i.ReadU16 ();
  m_valid = true;
  switch (type)
    {
    case DSDVTYPE_DSDV:
    case DSDVTYPE_RREQ:
    case DSDVTYPE_RACK:
      {
        m_type = (MessageType) type;
        break;
      }
    default:
      m_valid = false;
    }
  uint32_t dist = i.GetDistanceFrom (start);
  NS_ASSERT (dist == GetSerializedSize ());
  return dist;
}

void
TypeHeader::Print (std::ostream &os) const
{
  switch (m_type)
    {
    case DSDVTYPE_DSDV:
      {
        os << "DSDV";
        break;
      }
    case DSDVTYPE_RREQ:
      {
        os << "RREQ";
        break;
      }
    case DSDVTYPE_RACK:
      {
        os << "RACK";
        break;
      }
    default:
      os << "UNKNOWN_TYPE";
    }
}

bool
TypeHeader::operator== (TypeHeader const & o) const
{
  return (m_type == o.m_type && m_valid == o.m_valid);
}

std::ostream &
operator<< (std::ostream & os, TypeHeader const & h)
{
  h.Print (os);
  return os;
}

//-----------------------------------------------------------------------------
// DSDV
//-----------------------------------------------------------------------------
NS_OBJECT_ENSURE_REGISTERED (DsdvHeader);

DsdvHeader::DsdvHeader (Ipv4Address dst, uint16_t hopCount, uint32_t dstSeqNo)
  : m_dst (dst),
    m_hopCount (hopCount),
    m_dstSeqNo (dstSeqNo)
{
}

DsdvHeader::~DsdvHeader ()
{
}

TypeId
DsdvHeader::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::effdsdv::DsdvHeader")
    .SetParent<Header> ()
    .SetGroupName ("EffDsdv")
    .AddConstructor<DsdvHeader> ();
  return tid;
}

TypeId
DsdvHeader::GetInstanceTypeId () const
{
  return GetTypeId ();
}

uint32_t
DsdvHeader::GetSerializedSize () const
{
  return 10;
}

void
DsdvHeader::Serialize (Buffer::Iterator i) const
{
  WriteTo (i, m_dst);
  i.WriteHtonU16 (m_hopCount);
  i.WriteHtonU32 (m_dstSeqNo);

}

uint32_t
DsdvHeader::Deserialize (Buffer::Iterator start)
{
  Buffer::Iterator i = start;

  ReadFrom (i, m_dst);
  m_hopCount = i.ReadNtohU16 ();
  m_dstSeqNo = i.ReadNtohU32 ();

  uint32_t dist = i.GetDistanceFrom (start);
  NS_ASSERT (dist == GetSerializedSize ());
  return dist;
}

std::ostream &
operator<< (std::ostream & os, DsdvHeader const & h)
{
  h.Print (os);
  return os;
}

void
DsdvHeader::Print (std::ostream &os) const
{
  os << "DestinationIpv4: " << m_dst
     << " Hopcount: " << m_hopCount
     << " SequenceNumber: " << m_dstSeqNo;
}
//-----------------------------------------------------------------------------
// RREQ
//-----------------------------------------------------------------------------
NS_OBJECT_ENSURE_REGISTERED (RreqHeader);

RreqHeader::RreqHeader (Ipv4Address dst, uint8_t reserved)
  : m_dst (dst),
	m_reserved (reserved)
{
}

RreqHeader::~RreqHeader ()
{
}

TypeId
RreqHeader::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::effdsdv::RreqHeader")
    .SetParent<Header> ()
    .SetGroupName ("EffDsdv")
    .AddConstructor<DsdvHeader> ();
  return tid;
}

TypeId
RreqHeader::GetInstanceTypeId () const
{
  return GetTypeId ();
}

uint32_t
RreqHeader::GetSerializedSize () const
{
  return 6;
}

void
RreqHeader::Serialize (Buffer::Iterator i) const
{
  WriteTo (i, m_dst);
  i.WriteU16 (m_reserved);
}

uint32_t
RreqHeader::Deserialize (Buffer::Iterator start)
{
  Buffer::Iterator i = start;
  ReadFrom (i, m_dst);
  m_reserved = i.ReadU16 ();
  uint32_t dist = i.GetDistanceFrom (start);
  NS_ASSERT (dist == GetSerializedSize ());
  return dist;
}
std::ostream &
operator<< (std::ostream & os, RreqHeader const & h)
{
  h.Print (os);
  return os;
}

void
RreqHeader::Print (std::ostream &os) const
{
  os << "DestinationIpv4: " << m_dst;
}
//-----------------------------------------------------------------------------
// RACK
//-----------------------------------------------------------------------------
NS_OBJECT_ENSURE_REGISTERED (RackHeader);

RackHeader::RackHeader (Ipv4Address dst, uint16_t hopCount, Time updateTime)
  : m_dst (dst),
    m_hopCount (hopCount)
	{
	m_updateTime = uint32_t (updateTime.GetMilliSeconds ());
	}

RackHeader::~RackHeader ()
{
}

TypeId
RackHeader::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::effdsdv::RackHeader")
    .SetParent<Header> ()
    .SetGroupName ("EffDsdv")
    .AddConstructor<DsdvHeader> ();
  return tid;
}

TypeId
RackHeader::GetInstanceTypeId () const
{
  return GetTypeId ();
}

uint32_t
RackHeader::GetSerializedSize () const
{
  return 10;
}

void
RackHeader::Serialize (Buffer::Iterator i) const
{
  WriteTo (i, m_dst);
  i.WriteHtonU16 (m_hopCount);
  i.WriteHtonU32 (m_updateTime);

}

uint32_t
RackHeader::Deserialize (Buffer::Iterator start)
{
  Buffer::Iterator i = start;

  ReadFrom (i, m_dst);
  m_hopCount = i.ReadNtohU16 ();
  m_updateTime = i.ReadNtohU32 ();

  uint32_t dist = i.GetDistanceFrom (start);
  NS_ASSERT (dist == GetSerializedSize ());
  return dist;
}

std::ostream &
operator<< (std::ostream & os, RackHeader const & h)
{
  h.Print (os);
  return os;
}
void
RackHeader::Print (std::ostream &os) const
{
  os << "DestinationIpv4: " << m_dst
     << " Hopcount: " << m_hopCount
     << " UpdateTime: " << m_updateTime;
}

}
}
