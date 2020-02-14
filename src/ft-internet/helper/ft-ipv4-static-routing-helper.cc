/*
 * This class is based on ns3::Ipv4StaticRoutingHelper with slight modifications:
 * - a helper class to create ns3::FtIpv4StaticRouting instead of ns3::Ipv4StaticRouting objects
 * - note: multicast part is not modified
 */

/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2009 University of Washington
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
 */

#include <vector>
#include "ns3/log.h"
#include "ns3/ptr.h"
#include "ns3/names.h"
#include "ns3/node.h"
#include "ns3/ipv4.h"
#include "ns3/ipv4-route.h"
#include "ns3/ipv4-list-routing.h"
#include "ns3/assert.h"
#include "ns3/ipv4-address.h"
#include "ns3/ipv4-routing-protocol.h"
#include "ft-ipv4-static-routing-helper.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("FtIpv4StaticRoutingHelper");

FtIpv4StaticRoutingHelper::FtIpv4StaticRoutingHelper()
{
}

FtIpv4StaticRoutingHelper::FtIpv4StaticRoutingHelper (const FtIpv4StaticRoutingHelper &o)
{
}

FtIpv4StaticRoutingHelper*
FtIpv4StaticRoutingHelper::Copy (void) const
{
  return new FtIpv4StaticRoutingHelper (*this);
}

Ptr<Ipv4RoutingProtocol>
FtIpv4StaticRoutingHelper::Create (Ptr<Node> node) const
{
  return CreateObject<FtIpv4StaticRouting> ();
}


Ptr<FtIpv4StaticRouting>
FtIpv4StaticRoutingHelper::GetStaticRouting (Ptr<Ipv4> ipv4) const
{
  NS_LOG_FUNCTION (this);
  Ptr<Ipv4RoutingProtocol> ipv4rp = ipv4->GetRoutingProtocol ();
  NS_ASSERT_MSG (ipv4rp, "No routing protocol associated with Ipv4");
  if (DynamicCast<FtIpv4StaticRouting> (ipv4rp))
    {
      NS_LOG_LOGIC ("Static routing found as the main IPv4 routing protocol.");
      return DynamicCast<FtIpv4StaticRouting> (ipv4rp);
    } 
    
  if (DynamicCast<Ipv4ListRouting> (ipv4rp))
    {
      Ptr<Ipv4ListRouting> lrp = DynamicCast<Ipv4ListRouting> (ipv4rp);
      int16_t priority;
      for (uint32_t i = 0; i < lrp->GetNRoutingProtocols ();  i++)
        {
          NS_LOG_LOGIC ("Searching for static routing in list");
          Ptr<Ipv4RoutingProtocol> temp = lrp->GetRoutingProtocol (i, priority);
          if (DynamicCast<FtIpv4StaticRouting> (temp))
            {
              NS_LOG_LOGIC ("Found static routing in list");
              return DynamicCast<FtIpv4StaticRouting> (temp);
            }
        }
    }
  NS_LOG_LOGIC ("Static routing not found");
  return 0;
}

void
FtIpv4StaticRoutingHelper::AddMulticastRoute (
  Ptr<Node> n,
  Ipv4Address source, 
  Ipv4Address group,
  Ptr<NetDevice> input, 
  NetDeviceContainer output)
{
  Ptr<Ipv4> ipv4 = n->GetObject<Ipv4> ();

  // We need to convert the NetDeviceContainer to an array of interface 
  // numbers
  std::vector<uint32_t> outputInterfaces;
  for (NetDeviceContainer::Iterator i = output.Begin (); i != output.End (); ++i)
    {
      Ptr<NetDevice> nd = *i;
      int32_t interface = ipv4->GetInterfaceForDevice (nd);
      NS_ASSERT_MSG (interface >= 0,
                     "FtIpv4StaticRoutingHelper::AddMulticastRoute(): "
                     "Expected an interface associated with the device nd");
      outputInterfaces.push_back (interface);
    }

  int32_t inputInterface = ipv4->GetInterfaceForDevice (input);
  NS_ASSERT_MSG (inputInterface >= 0,
                 "FtIpv4StaticRoutingHelper::AddMulticastRoute(): "
                 "Expected an interface associated with the device input");
  FtIpv4StaticRoutingHelper helper;
  Ptr<FtIpv4StaticRouting> ftIpv4StaticRouting = helper.GetStaticRouting (ipv4);
  if (!ftIpv4StaticRouting)
    {
      NS_ASSERT_MSG (ftIpv4StaticRouting,
                     "FtIpv4StaticRoutingHelper::SetDefaultMulticastRoute(): "
                     "Expected an Ipv4StaticRouting associated with this node");
    }
  ftIpv4StaticRouting->AddMulticastRoute (source, group, inputInterface, outputInterfaces);
}

void
FtIpv4StaticRoutingHelper::AddMulticastRoute (
  Ptr<Node> n,
  Ipv4Address source, 
  Ipv4Address group,
  std::string inputName, 
  NetDeviceContainer output)
{
  Ptr<NetDevice> input = Names::Find<NetDevice> (inputName);
  AddMulticastRoute (n, source, group, input, output);
}

void
FtIpv4StaticRoutingHelper::AddMulticastRoute (
  std::string nName,
  Ipv4Address source, 
  Ipv4Address group,
  Ptr<NetDevice> input, 
  NetDeviceContainer output)
{
  Ptr<Node> n = Names::Find<Node> (nName);
  AddMulticastRoute (n, source, group, input, output);
}

void
FtIpv4StaticRoutingHelper::AddMulticastRoute (
  std::string nName,
  Ipv4Address source, 
  Ipv4Address group,
  std::string inputName, 
  NetDeviceContainer output)
{
  Ptr<NetDevice> input = Names::Find<NetDevice> (inputName);
  Ptr<Node> n = Names::Find<Node> (nName);
  AddMulticastRoute (n, source, group, input, output);
}

void
FtIpv4StaticRoutingHelper::SetDefaultMulticastRoute (
  Ptr<Node> n, 
  Ptr<NetDevice> nd)
{
  Ptr<Ipv4> ipv4 = n->GetObject<Ipv4> ();
  int32_t interfaceSrc = ipv4->GetInterfaceForDevice (nd);
  NS_ASSERT_MSG (interfaceSrc >= 0,
                 "FtIpv4StaticRoutingHelper::SetDefaultMulticastRoute(): "
                 "Expected an interface associated with the device");
  FtIpv4StaticRoutingHelper helper;
  Ptr<FtIpv4StaticRouting> ftIpv4StaticRouting = helper.GetStaticRouting (ipv4);
  if (!ftIpv4StaticRouting)
    {
      NS_ASSERT_MSG (ftIpv4StaticRouting,
                     "FtIpv4StaticRoutingHelper::SetDefaultMulticastRoute(): "
                     "Expected an Ipv4StaticRouting associated with this node");
    }
  ftIpv4StaticRouting->SetDefaultMulticastRoute (interfaceSrc);
}

void
FtIpv4StaticRoutingHelper::SetDefaultMulticastRoute (
  Ptr<Node> n, 
  std::string ndName)
{
  Ptr<NetDevice> nd = Names::Find<NetDevice> (ndName);
  SetDefaultMulticastRoute (n, nd);
}

void
FtIpv4StaticRoutingHelper::SetDefaultMulticastRoute (
  std::string nName, 
  Ptr<NetDevice> nd)
{
  Ptr<Node> n = Names::Find<Node> (nName);
  SetDefaultMulticastRoute (n, nd);
}

void
FtIpv4StaticRoutingHelper::SetDefaultMulticastRoute (
  std::string nName, 
  std::string ndName)
{
  Ptr<Node> n = Names::Find<Node> (nName);
  Ptr<NetDevice> nd = Names::Find<NetDevice> (ndName);
  SetDefaultMulticastRoute (n, nd);
}

} // namespace ns3
