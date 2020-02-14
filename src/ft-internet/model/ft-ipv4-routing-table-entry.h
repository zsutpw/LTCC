/*
 * This class is based on ns3::Ipv4RoutingTableEntry with slight modifications:
 * - additional parameter uint32_t flowId in the entry
 * - multicast part is not modified (only class name FtIpv4MulticastRoutingTableEntry)
 */

/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2005 INRIA
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
 * Author: Mathieu Lacage <mathieu.lacage@sophia.inria.fr>
 */
#ifndef FT_IPV4_ROUTING_TABLE_ENTRY_H
#define Ft_IPV4_ROUTING_TABLE_ENTRY_H

#include <list>
#include <vector>
#include <ostream>

#include "ns3/ipv4-address.h"

namespace ns3 {

/**
 * \ingroup ipv4Routing
 *
 * A record of an IPv4 routing table entry for Ipv4GlobalRouting and 
 * Ipv4StaticRouting.  This is not a reference counted object.
 */
class FtIpv4RoutingTableEntry {
public:
  /**
   * \brief This constructor does nothing
   */
  FtIpv4RoutingTableEntry ();
  /**
   * \brief Copy Constructor
   * \param route The route to copy
   */
  FtIpv4RoutingTableEntry (FtIpv4RoutingTableEntry const &route);
  /**
   * \brief Copy Constructor
   * \param route The route to copy
   */
  FtIpv4RoutingTableEntry (FtIpv4RoutingTableEntry const *route);
  /**
   * \return True if this route is a host route (mask of all ones); false otherwise
   */
  bool IsHost (void) const;
  /**
   * \return True if this route is not a host route (mask is not all ones); false otherwise
   *
   * This method is implemented as !IsHost ().
   */
  bool IsNetwork (void) const;
  /**
   * \return True if this route is a default route; false otherwise
   */
  bool IsDefault (void) const;
  /**
   * \return True if this route is a gateway route; false otherwise
   */
  bool IsGateway (void) const;
  /**
   * \return address of the gateway stored in this entry
   */
  Ipv4Address GetGateway (void) const;
  /**
   * \return The IPv4 address of the destination of this route
   */
  Ipv4Address GetDest (void) const;
  /**
   * \return The IPv4 network number of the destination of this route
   */
  Ipv4Address GetDestNetwork (void) const;
  /**
   * \return The IPv4 network mask of the destination of this route
   */
  Ipv4Mask GetDestNetworkMask (void) const;
  /**
   * \return The Ipv4 interface number used for sending outgoing packets
   */
  uint32_t GetInterface (void) const;
  /**
   * \return The flow id
   */
  uint32_t GetFlowId (void) const;
  

  /**
   * \return An FtIpv4RoutingTableEntry object corresponding to the input parameters.
   * \param dest Ipv4Address of the destination
   * \param nextHop Ipv4Address of the next hop
   * \param interface Outgoing interface 
   */
  static FtIpv4RoutingTableEntry CreateHostRouteTo (Ipv4Address dest,
                                                  Ipv4Address nextHop,
                                                  uint32_t interface);
  /**
   * \return An FtIpv4RoutingTableEntry object corresponding to the input parameters.
   * \param dest Ipv4Address of the destination
   * \param interface Outgoing interface 
   */
  static FtIpv4RoutingTableEntry CreateHostRouteTo (Ipv4Address dest,
                                                  uint32_t interface);
  /**
   * \return An FtIpv4RoutingTableEntry object corresponding to the input parameters.
   * \param network Ipv4Address of the destination network
   * \param networkMask Ipv4Mask of the destination network mask
   * \param nextHop Ipv4Address of the next hop
   * \param interface Outgoing interface 
   */
  static FtIpv4RoutingTableEntry CreateNetworkRouteTo (Ipv4Address network,
                                                     Ipv4Mask networkMask,
                                                     Ipv4Address nextHop,
                                                     uint32_t interface);
  /**
   * \return An FtIpv4RoutingTableEntry object corresponding to the input parameters.
   * \param network Ipv4Address of the destination network
   * \param networkMask Ipv4Mask of the destination network mask
   * \param interface Outgoing interface 
   */
  static FtIpv4RoutingTableEntry CreateNetworkRouteTo (Ipv4Address network,
                                                     Ipv4Mask networkMask,
                                                     uint32_t interface);
  /**
   * \return An FtIpv4RoutingTableEntry object corresponding to the input
   * parameters.  This route is distinguished; it will match any 
   * destination for which a more specific route does not exist.
   * \param nextHop Ipv4Address of the next hop
   * \param interface Outgoing interface 
   */
  static FtIpv4RoutingTableEntry CreateDefaultRoute (Ipv4Address nextHop,
                                                   uint32_t interface);

  /**
   * \return An FtIpv4RoutingTableEntry object corresponding to the input parameters.
   * \param flow id
   * \param network Ipv4Address of the destination network
   * \param networkMask Ipv4Mask of the destination network mask
   * \param nextHop Ipv4Address of the next hop
   * \param interface Outgoing interface 
   */
  static FtIpv4RoutingTableEntry CreateNetworkRouteTo (uint32_t flowId,
                                                     Ipv4Address network, 
                                                     Ipv4Mask networkMask,
                                                     Ipv4Address nextHop,
                                                     uint32_t interface);

private:
  /**
   * \brief Constructor.
   * \param network network address
   * \param mask network mask
   * \param gateway the gateway
   * \param interface the interface index
   */
  FtIpv4RoutingTableEntry (Ipv4Address network,
                         Ipv4Mask mask,
                         Ipv4Address gateway,
                         uint32_t interface);
  /**
   * \brief Constructor.
   * \param dest destination address
   * \param mask network mask
   * \param interface the interface index
   */
  FtIpv4RoutingTableEntry (Ipv4Address dest,
                         Ipv4Mask mask,
                         uint32_t interface);
  /**
   * \brief Constructor.
   * \param dest destination address
   * \param gateway the gateway
   * \param interface the interface index
   */
  FtIpv4RoutingTableEntry (Ipv4Address dest,
                         Ipv4Address gateway,
                         uint32_t interface);
  /**
   * \brief Constructor.
   * \param dest destination address
   * \param interface the interface index
   */
  FtIpv4RoutingTableEntry (Ipv4Address dest,
                         uint32_t interface);

  /**
   * \brief Ft constructor with flow id.
   * \param flow id
   * \param dest destination address
   * \param mask network mask
   * \param gateway the gateway
   * \param interface the interface index
   */
  FtIpv4RoutingTableEntry (uint32_t flowId,
                         Ipv4Address dest,
                         Ipv4Mask mask,
                         Ipv4Address gateway,
                         uint32_t interface);

  uint32_t m_flowId;          //!< ft flow id
  Ipv4Address m_dest;         //!< destination address
  Ipv4Mask m_destNetworkMask; //!< destination network mask
  Ipv4Address m_gateway;      //!< gateway
  uint32_t m_interface;       //!< output interface

};

/**
 * \brief Stream insertion operator.
 *
 * \param os the reference to the output stream
 * \param route the Ipv4 routing table entry
 * \returns the reference to the output stream
 */
std::ostream& operator<< (std::ostream& os, FtIpv4RoutingTableEntry const& route);

/**
 * \brief Equality operator.
 *
 * \param a lhs
 * \param b rhs
 * \returns true if operands are equal, false otherwise
 */
bool operator== (const FtIpv4RoutingTableEntry a, const FtIpv4RoutingTableEntry b);

/**
 * \ingroup ipv4Routing
 *
 * \brief A record of an IPv4 multicast route for Ipv4GlobalRouting and Ipv4StaticRouting
 */
class FtIpv4MulticastRoutingTableEntry {
public:
  /**
   * \brief This constructor does nothing
   */
  FtIpv4MulticastRoutingTableEntry ();

  /**
   * \brief Copy Constructor
   * \param route The route to copy
   */
  FtIpv4MulticastRoutingTableEntry (FtIpv4MulticastRoutingTableEntry const &route);
  /**
   * \brief Copy Constructor
   * \param route The route to copy
   */
  FtIpv4MulticastRoutingTableEntry (FtIpv4MulticastRoutingTableEntry const *route);
  /**
   * \return The IPv4 address of the source of this route
   */
  Ipv4Address GetOrigin (void) const;
  /**
   * \return The IPv4 address of the multicast group of this route
   */
  Ipv4Address GetGroup (void) const;
  /**
   * \return The IPv4 address of the input interface of this route
   */
  uint32_t GetInputInterface (void) const;
  /**
   * \return The number of output interfaces of this route
   */
  uint32_t GetNOutputInterfaces (void) const;
  /**
   * \param n interface index
   * \return A specified output interface.
   */
  uint32_t GetOutputInterface (uint32_t n) const;
  /**
   * \return A vector of all of the output interfaces of this route.
   */
  std::vector<uint32_t> GetOutputInterfaces (void) const;
  /**
   * \return FtIpv4MulticastRoutingTableEntry corresponding to the input parameters.
   * \param origin Source address for the multicast route 
   * \param group Group destination address for the multicast route
   * \param inputInterface Input interface that multicast datagram must be received on
   * \param outputInterfaces vector of output interfaces to copy and forward the datagram to
   */
  static FtIpv4MulticastRoutingTableEntry CreateMulticastRoute (Ipv4Address origin,
                                                              Ipv4Address group, uint32_t inputInterface,
                                                              std::vector<uint32_t> outputInterfaces);

private:
  /**
   * \brief Constructor.
   * \param origin source address
   * \param group destination address
   * \param inputInterface input interface
   * \param outputInterfaces output interfaces
   */
  FtIpv4MulticastRoutingTableEntry (Ipv4Address origin, Ipv4Address group,
                                  uint32_t inputInterface, std::vector<uint32_t> outputInterfaces);

  Ipv4Address m_origin;   //!< source address
  Ipv4Address m_group;    //!< destination address
  uint32_t m_inputInterface;    //!< input interface
  std::vector<uint32_t> m_outputInterfaces;   //!< output interfaces
};

/**
 * \brief Stream insertion operator.
 *
 * \param os the reference to the output stream
 * \param route the Ipv4 multicast routing table entry
 * \returns the reference to the output stream
 */
std::ostream& operator<< (std::ostream& os, FtIpv4MulticastRoutingTableEntry const& route);

/**
 * \brief Equality operator.
 *
 * \param a lhs
 * \param b rhs
 * \returns true if operands are equal, false otherwise
 */
bool operator== (const FtIpv4MulticastRoutingTableEntry a, const FtIpv4MulticastRoutingTableEntry b);

} // namespace ns3

#endif /* IPV4_ROUTING_TABLE_ENTRY_H */
