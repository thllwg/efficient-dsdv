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

#ifndef EFFDSDV_PACKET_H
#define EFFDSDV_PACKET_H

#include <iostream>
#include "ns3/header.h"
#include "ns3/ipv4-address.h"
#include "ns3/nstime.h"
#include "ns3/enum.h"

namespace ns3 {
namespace effdsdv {

/**
* \ingroup aodv
* \brief MessageType enumeration
*/
enum MessageType
{
  DSDVTYPE_DSDV  = 1,   //!< AODVTYPE_DSDV
  DSDVTYPE_RREQ  = 2,   //!< AODVTYPE_RREP
  DSDVTYPE_RACK  = 3,   //!< AODVTYPE_RERR
  	  	  	  	  	  	 //!< 4 = Undefined
};

/**
* \ingroup aodv
* \brief AODV types
*/
class TypeHeader : public Header
{
public:
  /**
   * constructor
   * \param t the EffDSDV Message type
   */
  TypeHeader (MessageType t = DSDVTYPE_DSDV);

  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId ();
  TypeId GetInstanceTypeId () const;
  uint32_t GetSerializedSize () const;
  void Serialize (Buffer::Iterator start) const;
  uint32_t Deserialize (Buffer::Iterator start);
  void Print (std::ostream &os) const;

  /**
   * \returns the type
   */
  MessageType Get () const
  {
    return m_type;
  }
  /**
   * Check that type is valid
   * returns true if the type is valid
   */
  bool IsValid () const
  {
    return m_valid;
  }
  /**
   * \brief Comparison operator
   * \param o header to compare
   * \return true if the headers are equal
   */
  bool operator== (TypeHeader const & o) const;
private:
  MessageType m_type; ///< type of the message
  bool m_valid; ///< Indicates if the message is valid
};

/**
  * \brief Stream output operator
  * \param os output stream
  * \return updated stream
  */
std::ostream & operator<< (std::ostream & os, TypeHeader const & h);

/**
 * \ingroup effdsdv
 * \brief EFFDSDV_DSDV Update Packet Format.
 * \verbatim
 |      0        |      1        |      2        |       3       |
  0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |              TYPE             |         Destination ----------
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   ---------- Address            |            HopCount           |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |                       Sequence Number                         |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * \endverbatim
 */

class DsdvHeader : public Header
{
public:
  /**
   * Constructor
   *
   * \param dst destination IP address
   * \param hopcount hop count
   * \param dstSeqNo destination sequence number
   */
  DsdvHeader (Ipv4Address dst = Ipv4Address (), uint16_t hopcount = 0, uint32_t dstSeqNo = 0);
  virtual ~DsdvHeader ();
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);
  virtual TypeId GetInstanceTypeId (void) const;
  virtual uint32_t GetSerializedSize () const;
  virtual void Serialize (Buffer::Iterator start) const;
  virtual uint32_t Deserialize (Buffer::Iterator start);
  void Print (std::ostream &os) const;

  /**
   * Set destination address
   * \param destination the destination IPv4 address
   */
  void
  SetDst (Ipv4Address destination)
  {
    m_dst = destination;
  }
  /**
   * Get destination address
   * \returns the destination IPv4 address
   */
  Ipv4Address
  GetDst () const
  {
    return m_dst;
  }
  /**
   * Set hop count
   * \param hopCount the hop count
   */
  void
  SetHopCount (uint16_t hopCount)
  {
    m_hopCount = hopCount;
  }
  /**
   * Get hop count
   * \returns the hop count
   */
  uint16_t
  GetHopCount () const
  {
    return m_hopCount;
  }
  /**
   * Set destination sequence number
   * \param sequenceNumber The sequence number
   */
  void
  SetDstSeqno (uint32_t sequenceNumber)
  {
    m_dstSeqNo = sequenceNumber;
  }
  /**
   * Get destination sequence number
   * \returns the destination sequence number
   */
  uint32_t
  GetDstSeqno () const
  {
    return m_dstSeqNo;
  }
private:
  Ipv4Address m_dst; ///< Destination IP Address
  uint16_t m_hopCount; ///< Number of Hops
  uint32_t m_dstSeqNo; ///< Destination Sequence Number
};
/**
* \brief Stream output operator
* \param os output stream
* \return updated stream
*/
std::ostream & operator<< (std::ostream & os, DsdvHeader const &);

/**
 * \ingroup effdsdv
 * \brief EFFDSDV_RREQ Update Packet Format.
 * \verbatim
 |      0        |      1        |      2        |       3       |
  0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |      TYPE                     |               Destination Add-
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  ress                           |            Reserved           |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * \endverbatim
 */

class RreqHeader : public Header
{
public:
  /**
   * Constructor
   *
   * \param type packet type (Rreq)
   * \param dst destination IP address
   */
	RreqHeader (Ipv4Address dst = Ipv4Address (), uint8_t reserved = 0);
  virtual ~RreqHeader ();
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);
  virtual TypeId GetInstanceTypeId (void) const;
  virtual uint32_t GetSerializedSize () const;
  virtual void Serialize (Buffer::Iterator start) const;
  virtual uint32_t Deserialize (Buffer::Iterator start);
  void Print (std::ostream &os) const;

  /**
   * Set destination address
   * \param destination the destination IPv4 address
   */
  void
  SetDst (Ipv4Address destination)
  {
    m_dst = destination;
  }
  /**
   * Get destination address
   * \returns the destination IPv4 address
   */
  Ipv4Address
  GetDst () const
  {
    return m_dst;
  }

private:
  Ipv4Address m_dst; ///< Destination IP Address
  uint8_t     m_reserved;       ///< Not used (must be 0)
};
/**
* \brief Stream output operator
* \param os output stream
* \return updated stream
*/
std::ostream & operator<< (std::ostream & os, RreqHeader const &);
/**
 * \ingroup effdsdv
 * \brief EFFDSDV_RACK Update Packet Format.
 * \verbatim
 |      0        |      1        |      2        |       3       |
  0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |      TYPE                     |               Destination Add-
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  ress                           |            HopCount           |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |                       UPDATE TIME                             |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * \endverbatim
 */

class RackHeader : public Header
{
public:
  /**
   * Constructor
   *
   * \param dst destination IP address
   * \param hopcount hop count
   * \param dstSeqNo destination sequence number
   */
  RackHeader (Ipv4Address dst = Ipv4Address (), uint16_t hopcount = 0, Time updatetime = MilliSeconds (0));
  virtual ~RackHeader ();
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);
  virtual TypeId GetInstanceTypeId (void) const;
  virtual uint32_t GetSerializedSize () const;
  virtual void Serialize (Buffer::Iterator start) const;
  virtual uint32_t Deserialize (Buffer::Iterator start);
  void Print (std::ostream &os) const;

  /**
   * Set destination address
   * \param destination the destination IPv4 address
   */
  void
  SetDst (Ipv4Address destination)
  {
    m_dst = destination;
  }
  /**
   * Get destination address
   * \returns the destination IPv4 address
   */
  Ipv4Address
  GetDst () const
  {
    return m_dst;
  }
  /**
   * Set hop count
   * \param hopCount the hop count
   */
  void
  SetHopCount (uint16_t hopCount)
  {
    m_hopCount = hopCount;
  }
  /**
   * Get hop count
   * \returns the hop count
   */
  uint16_t
  GetHopCount () const
  {
    return m_hopCount;
  }
  /**
   * \brief Set the lifetime
   * \param t the lifetime
   */
  void
  SetUpdateTime (Time t){
	  m_updateTime = t.GetMilliSeconds ();
  }
  /**
   * \brief Get the lifetime
   * \return the lifetime
   */
  Time GetUpdateTime () const
  {
	  Time t (MilliSeconds (m_updateTime));
	    return t;
  }

private:
  Ipv4Address m_dst; ///< Destination IP Address
  uint16_t m_hopCount; ///< Number of Hops
  uint32_t m_updateTime; ///< Destination Sequence Number
};
/**
* \brief Stream output operator
* \param os output stream
* \return updated stream
*/
std::ostream & operator<< (std::ostream & os, RackHeader const &);

}
}

#endif /* EFFDSDV_PACKET_H */
