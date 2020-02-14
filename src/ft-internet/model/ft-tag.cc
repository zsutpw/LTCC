/*
 * This class is based on main-packet-tag.cc example with slight modifications:
 * - a uint32_t tag that is added to all packets generated by FtOnOffApplication
 * - used in routing to route by flowId
 */

#include "ns3/log.h"
#include "ns3/ptr.h"
#include "ns3/names.h"
#include "ns3/assert.h"
#include "ns3/tag.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"
#include "ft-tag.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("FtTag");

TypeId 
FtTag::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::FtTag")
    .SetParent<Tag> ()
    .AddConstructor<FtTag> ()
    .AddAttribute ("SimpleValue",
                   "A simple value",
                   EmptyAttributeValue (),
                   MakeUintegerAccessor (&FtTag::GetSimpleValue),
                   MakeUintegerChecker<uint32_t> ())
  ;
  return tid;
}

TypeId 
FtTag::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}

uint32_t 
FtTag::GetSerializedSize (void) const
{
  return 4;
}

void 
FtTag::Serialize (TagBuffer i) const
{
  i.WriteU32 (m_simpleValue);
}

void 
FtTag::Deserialize (TagBuffer i)
{
  m_simpleValue = i.ReadU32 ();
}

void 
FtTag::Print (std::ostream &os) const
{
  os << "v=" << (uint32_t)m_simpleValue;
}

void 
FtTag::SetSimpleValue (uint32_t value)
{
  m_simpleValue = value;
}

uint32_t 
FtTag::GetSimpleValue (void) const
{
  return m_simpleValue;
}

} // namespace ns3