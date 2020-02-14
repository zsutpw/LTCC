#include <iostream>
#include <fstream>
#include <string>
#include <cassert>

#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/ft-internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/queue.h"
#include "ns3/drop-tail-queue.h"
#include "ns3/config-store-module.h"

#include <tuple>
#include <vector>
#include <utility> //for pair
#include <sys/time.h>

// get real time in sec
double get_time(){
  timeval tv;
  gettimeofday(&tv, NULL);
  return tv.tv_sec + tv.tv_usec * 1e-6;
}

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("FlowThinning");

void 
EnableLogComponents()
{
  LogComponentEnable ("FlowThinning", LOG_ALL);
  
  //LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
  //LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);
  //LogComponentEnable ("UdpEchoClientApplication", LOG_ALL);
  //LogComponentEnable ("UdpEchoServerApplication", LOG_ALL);
  //LogComponentEnable ("PointToPointNetDevice", LOG_ALL);
  //LogComponentEnable ("Config", LOG_ALL);
  //LogComponentEnable ("Config", LOG_LEVEL_ALL);
  //LogComponentEnable ("Names", LOG_ALL);
  //LogComponentEnable ("FtOnOffApplication", LOG_ALL);
  //LogComponentEnable ("FtIpv4StaticRouting", LOG_LEVEL_FUNCTION);
  //LogComponentEnable ("FtIpv4StaticRoutingHelper", LOG_ALL);
  //LogComponentEnable ("PacketSink", LOG_ALL);
  //LogComponentEnable ("Node", LOG_LEVEL_INFO);
  //LogComponentEnable ("Object", LOG_LEVEL_ALL);
  //LogComponentEnable ("Ipv4L3Protocol", LOG_LEVEL_ALL);
  //LogComponentEnable ("FlowMonitor", LOG_LEVEL_ALL);
  //LogComponentEnable ("Queue", LOG_LEVEL_LOGIC);
}

void 
ChangeBandwidth(const StringValue & dataRateValue, NetDeviceContainer & devices)
{
  devices.Get(0)->SetAttribute("DataRate", dataRateValue);
  devices.Get(1)->SetAttribute("DataRate", dataRateValue);

  StringValue dr0;
  devices.Get(0)->GetAttribute("DataRate", dr0);
  NS_LOG_INFO("-------after datarate 0 " << dr0.Get());
  StringValue dr1;
  devices.Get(1)->GetAttribute("DataRate", dr1);
  NS_LOG_INFO("-------after datarate 1 " << dr1.Get());
}

void 
ScheduleDataRateChange(const Time & scheduleTime, const StringValue & dataRateValue, NetDeviceContainer & devices)
{
  Simulator::Schedule (scheduleTime, &ChangeBandwidth, dataRateValue, devices);
}

// When scheduling app datarate change, first change datarate value
// then turn off and almost immediately turn it on.
// This way datarate changes like 0->x or x->0 will work properly.
void 
ChangeTrafficRate(const StringValue & trafficRateValue, Ptr<Application> application, const Time & scheduleTime)
{
  Ptr<FtOnOffApplication> onOffApp = DynamicCast<FtOnOffApplication> (application);
  onOffApp->ScheduleStopAppOnTime(NanoSeconds(10.0));
  application->SetAttribute("DataRate", StringValue(trafficRateValue));
  StringValue dr0;
  onOffApp->GetAttribute("DataRate", dr0);
  NS_LOG_INFO("-------after app datarate arg " << trafficRateValue.Get());
  NS_LOG_INFO("-------after app datarate " << dr0.Get());
  onOffApp->ScheduleStartAppOnTime(NanoSeconds(20.0)); 
}

void 
ScheduleTrafficRateChange(const Time & scheduleTime, const StringValue & trafficRateValue, Ptr<Application> application)
{
  Simulator::Schedule (scheduleTime, &ChangeTrafficRate, trafficRateValue, application, scheduleTime); 
}

void
SaveRoutingTablesToFile(const std::string & fileName)
{
  Ipv4GlobalRoutingHelper gr;
  //Ipv4StaticRoutingHelper sr;
  Ptr<OutputStreamWrapper> outputStream = Create<OutputStreamWrapper> (fileName, std::ios::out);
  gr.PrintRoutingTableAllAt(Seconds(6), outputStream);
  //sr.PrintRoutingTableAllAt(Seconds(6), outputStream);

  //Ipv4ListRouting list;
  //list.PrintRoutingTable(outputStream, ns3::Time::MS);
}

void 
MyTagTest()
{
  FtTag tag;
  tag.SetSimpleValue(56);
  Ptr<Packet> packet = Create<Packet> (100);
  packet->AddPacketTag(tag); 

  PacketTagIterator::Item item = (packet->GetPacketTagIterator()).Next();
  Callback<ObjectBase *> constructor = item.GetTypeId().GetConstructor();
  FtTag *tg = dynamic_cast<FtTag *> (constructor());
  item.GetTag(*tg);
  delete tg;

  NS_LOG_INFO("-------packet tag value =  " << (uint32_t)tg->GetSimpleValue());
}

/*
void 
MyByteTagTest()
{
  FtTag tag;
  tag.SetSimpleValue(56);
  Ptr<Packet> packet = Create<Packet> (100);
  packet->AddByteTag(tag); 

  FtTag tg;
  if(packet->FindFirstMatchingByteTag(tg)){
    NS_LOG_INFO("------- packet byte tag value = " << (uint32_t)tg.GetSimpleValue());
  }
}
*/

void
run()
{
  //MyTagTest();
  //MyByteTagTest();

  //create nodes
  NodeContainer allNodes;
  allNodes.Create (3);
  NodeContainer n0n1 = NodeContainer(allNodes.Get(0), allNodes.Get(1));
  NodeContainer n1n2 = NodeContainer(allNodes.Get(1), allNodes.Get(2));
  NodeContainer n0n2 = NodeContainer(allNodes.Get(0), allNodes.Get(2));

  //Names::Add("/Names/node0", nodes.Get(0));
  //Names::Add("/Names/node1", nodes.Get(1));
  //NS_LOG_INFO("------------");  
  //NS_LOG_INFO(Names::FindName(nodes.Get(0)));
  //NS_LOG_INFO("------------");

  //create channels and devices
  //ber?, offset
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("20kbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("0ms"));
  

  NetDeviceContainer d0d1;
  d0d1 = pointToPoint.Install (n0n1);
  NetDeviceContainer d1d2;
  d1d2 = pointToPoint.Install (n1n2);
  NetDeviceContainer d0d2;
  d0d2 = pointToPoint.Install (n0n2);
  pointToPoint.EnablePcapAll("ft-pcap-tracer");

  //create routing
  FtIpv4StaticRoutingHelper ipv4RoutingHelper1;
  
  InternetStackHelper internetStack;
  internetStack.SetRoutingHelper(ipv4RoutingHelper1);
  internetStack.Install(allNodes);

  //assign addresses/subnets to each edge, order is important!
  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.252");
  Ipv4InterfaceContainer i0i1 = ipv4.Assign(d0d1);
  ipv4.SetBase("10.1.2.0", "255.255.255.252");
  Ipv4InterfaceContainer i1i2 = ipv4.Assign(d1d2);
  ipv4.SetBase("10.1.3.0", "255.255.255.252");
  Ipv4InterfaceContainer i0i2 = ipv4.Assign(d0d2);
  
  Ptr<Ipv4> ipv40 = (allNodes.Get(0))->GetObject<Ipv4>();
  Ptr<Ipv4> ipv41 = (allNodes.Get(1))->GetObject<Ipv4>();
  Ptr<Ipv4> ipv42 = (allNodes.Get(2))->GetObject<Ipv4>();
  
  Ptr<FtIpv4StaticRouting> staticRouting10 = ipv4RoutingHelper1.GetStaticRouting(ipv40);
  Ptr<FtIpv4StaticRouting> staticRouting11 = ipv4RoutingHelper1.GetStaticRouting(ipv41);
  Ptr<FtIpv4StaticRouting> staticRouting12 = ipv4RoutingHelper1.GetStaticRouting(ipv42);

  // add demand 0-2 and 2 paths, with different flowId's
  // path 0-1-2 
  uint32_t flowId = 43;
  staticRouting10->AddHostRouteTo(flowId, Ipv4Address("10.1.2.2"), Ipv4Address("10.1.1.2"), 1);
  staticRouting11->AddHostRouteTo(flowId, Ipv4Address("10.1.2.2"), Ipv4Address("10.1.2.2"), 2);

  // path 0-2
  uint32_t flowId2 = 55;
  staticRouting10->AddHostRouteTo(flowId2, Ipv4Address("10.1.3.2"), Ipv4Address("10.1.3.2"), 2);

  // create OnnOff application, sending, path 0-2, flowId = 43
  FtOnOffHelper onOff("ns3::UdpSocketFactory", InetSocketAddress(i1i2.GetAddress(1), 9));
  //NS_LOG_INFO(InetSocketAddress(i1i2.GetAddress(1), 9));
  onOff.SetConstantRate(DataRate("10kbps"));
  //onOff.SetAttribute("DataRate", StringValue("500kbps"));
  //total number of bytes to send, 0 means no limit
  //onOff.SetAttribute("MaxBytes", UintegerValue(1000));
  onOff.SetAttribute("FlowId", UintegerValue(43));
  onOff.SetAttribute("PacketSize", UintegerValue(25)); //bytes?
  ApplicationContainer apps = onOff.Install(allNodes.Get(0));
  apps.Start(Seconds(1.0));
  apps.Stop(Seconds(7.0));

  /*
  // create OnnOff application, sending, path 0-1-2, flowId = 55
  FtOnOffHelper onOff2("ns3::UdpSocketFactory", InetSocketAddress(i0i2.GetAddress(1), 9));
  onOff2.SetConstantRate(DataRate("1kbps"));
  //onOff2.SetAttribute("DataRate", StringValue("500kbps"));
  //total number of bytes to send, 0 means no limit
  //onOff2.SetAttribute("MaxBytes", UintegerValue(1000));
  onOff2.SetAttribute("FlowId", UintegerValue(55));
  onOff2.SetAttribute("PacketSize", UintegerValue(25)); //bytes?
  ApplicationContainer apps2 = onOff2.Install(allNodes.Get(0));
  apps2.Start(Seconds(1.0));
  apps2.Stop(Seconds(7.0));
  */

  // create PacketSink application, receiving
  PacketSinkHelper packetSink("ns3::UdpSocketFactory", Address(InetSocketAddress(Ipv4Address::GetAny(), 9))); //9 - port
  apps = packetSink.Install(allNodes.Get(2));
  apps.Start(Seconds(1.0));
  apps.Stop(Seconds(60.0)); 
  PacketSinkHelper packetSink2("ns3::UdpSocketFactory", Address(InetSocketAddress(Ipv4Address::GetAny(), 10)));
  apps.Add(packetSink2.Install(allNodes.Get(2)));
  apps.Start(Seconds(1.0));
  apps.Stop(Seconds(60.0)); 
  
  
  SaveRoutingTablesToFile("scratch/gr-routs.routes");

  /*
  StringValue datarate;
  d0d1.Get(0)->GetAttribute("DataRate", datarate);
  NS_LOG_INFO("-------before datarate " << datarate.Get());
  */

  // schedule events
  ScheduleDataRateChange(Seconds(3), StringValue("5kbps"), d0d1);
  ScheduleTrafficRateChange(Seconds(5), StringValue("5kbps"), (allNodes.Get(0))->GetApplication(0));


  FlowMonitorHelper flowMonitorHelper;
  //Ptr<FlowMonitor> flowMonitor = flowMonitorHelper.InstallAll(); 
  Ptr<FlowMonitor> flowMonitor = flowMonitorHelper.Install(allNodes);


  AsciiTraceHelper ascii;
  pointToPoint.EnableAsciiAll (ascii.CreateFileStream ("ft-ascii-tracer.tr"));

  //run simulation
  Simulator::Stop(Seconds(150));
  Simulator::Run ();
  
  flowMonitor->SerializeToXmlFile("scratch/flow_monitor_res.xml", true, true);

  flowMonitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowMonitorHelper.GetClassifier ());
  FlowMonitor::FlowStatsContainer stats = flowMonitor->GetFlowStats ();
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      std::cout << "Flow " << i->first - 2 << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
      std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
      std::cout << "  TxOffered:  " << i->second.txBytes * 8.0 / 9.0 / 1000 / 1000  << " Mbps\n";
      std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
      std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
      std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / 9.0 / 1000 / 1000  << " Mbps\n";
    }

  Simulator::Destroy ();
}

int
main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);
  
  Time::SetResolution (Time::NS);

  EnableLogComponents();

  NS_LOG_INFO("Hello!");

  run();

  return 0;
}

