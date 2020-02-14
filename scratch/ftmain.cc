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
#include <utility>
#include <sys/time.h>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("FlowThinning");


//////////////////////// utils
// get real time in sec
double get_time(){
  timeval tv;
  gettimeofday(&tv, NULL);
  return tv.tv_sec + tv.tv_usec * 1e-6;
}


//////////////////////// parameters with default values
// flow unit used in app datarate and edge bandwidth
std::string FLOW_UNIT = "bps";
// start all apps at this time 
double START_SIMULATION_TIME = 0.0;
// stop apps sending, recv app works till END_SIMULATION_TIME
double END_APPS_TIME = 20.0;
// stop simulation
double END_SIMULATION_TIME = 25.0;
// packet size given in bytes
int PACKET_SIZE = 1000;
// every STATS_DELTA_TIME seconds stats are taken from simulator
double STATS_DELTA_TIME = 0.01;
// scale all parameters with FLOW_UNIT: demand volumes, flow values, edge capacities..
double SCALE = 10000.0;
// multiply all edge bandwidth (initial and after events) by EDGE_BANDWIDTH_MARGIN
double EDGE_BANDWIDTH_MARGIN = 1.03;
// size of all point to point net devices queues [in packets]
int QUEUE_SIZE = 1000;
// transmission delay on link (point-to-point-channel) [in seconds]
double LINK_DELAY = 0.002;


// read parameters from file
// if no such file exists, default parameter values will be used
// parameters are expected to be given as follows:
//    name value
//    name value
//    ...
void
ReadParams(std::string filePath)
{
  NS_LOG_INFO("--- Read params from: " << filePath);
  std::ifstream fileStream(filePath);
  if(fileStream.is_open()){
    std::string parameterName;
    while(fileStream >> parameterName){
      std::string parameterValue;
      fileStream >> parameterValue;
      
      if(parameterName.compare("FLOW_UNIT") == 0){
        FLOW_UNIT = parameterValue;
      }else if(parameterName.compare("START_SIMULATION_TIME") == 0){
        START_SIMULATION_TIME = std::stod(parameterValue);
      }else if(parameterName.compare("END_APPS_TIME") == 0){
        END_APPS_TIME = std::stod(parameterValue);
      }else if(parameterName.compare("END_SIMULATION_TIME") == 0){
        END_SIMULATION_TIME = std::stod(parameterValue);
      }else if(parameterName.compare("PACKET_SIZE") == 0){
        PACKET_SIZE = std::stoi(parameterValue);
      }else if(parameterName.compare("STATS_DELTA_TIME") == 0){
        STATS_DELTA_TIME = std::stod(parameterValue);
      }else if(parameterName.compare("SCALE") == 0){
        SCALE = std::stod(parameterValue);
      }else if(parameterName.compare("EDGE_BANDWIDTH_MARGIN") == 0){
        EDGE_BANDWIDTH_MARGIN = std::stod(parameterValue);
      }else if(parameterName.compare("QUEUE_SIZE") == 0){
        QUEUE_SIZE = std::stoi(parameterValue);       
      }else if(parameterName.compare("LINK_DELAY") == 0){
        LINK_DELAY = std::stod(parameterValue);
      }else{
        NS_LOG_INFO("--- bad parameter name!!");
        break;
      }
    }
    fileStream.close();
  }else{
    NS_LOG_INFO("--- no params file: default parameter values will be used");
  }
  NS_LOG_INFO("--- End read params");
}


//////////////////////// graph
// number of vertecies
int V;
// number of edges
int E;
// graph edges as vector of pairs, first/second - node ids
std::vector<std::pair<int, int>> edges;
// initial bandwidths, FLOW_UNIT units are assumed 
std::vector<double> initial_edge_bandwidth;


// read graph information (V, E and edges) from file 
// format: first line V E, next E lines pairs of verticies v w
//         last line E values, bandwidths for all edges
void
ReadGraph(std::string filePath)
{
  NS_LOG_INFO("--- Read graph from: " << filePath);
  std::ifstream fileStream(filePath);
  if(fileStream.is_open()){
    fileStream >> V;
    fileStream >> E;
    initial_edge_bandwidth.resize(E);
    for(int edgeId = 0; edgeId < E; ++edgeId){
      int vertex1;
      int vertex2;
      fileStream >> vertex1 >> vertex2;
      edges.emplace_back(vertex1, vertex2);
    }
    for(int edgeId = 0; edgeId < E; ++edgeId){
      double bandwidth;
      fileStream >> bandwidth;
      initial_edge_bandwidth[edgeId] = bandwidth * SCALE * EDGE_BANDWIDTH_MARGIN;
    }
    fileStream.close();
  }else{
    NS_LOG_INFO("--- ERROR: GRAPH FILE NOT OPENED!!!");
  }
  NS_LOG_INFO("--- End read graph");
}


// print graph (V, E and edges) in LOG, format is the same as in ReadGraph
void
PrintGraph()
{
  NS_LOG_INFO("--- Print graph: ");
  NS_LOG_INFO(V << " " << E);
  for(int edgeId = 0; edgeId < E; ++edgeId){
    NS_LOG_INFO(edges[edgeId].first << " " << edges[edgeId].second);  
  }
  NS_LOG_INFO("--- End print graph: ");
}
//////////////////////// end graph


/////////////////////// demands/paths
// number of demands 
int D;
// o(d) - source node ids for demand specified by index 
std::vector<int> demand_fr;
// t(d) - destination node ids for demand specified by index
std::vector<int> demand_to;
// h(d) - demand volume
std::vector<double> demand_volume;
// P, paths for each demand as list of link ids
std::vector<std::vector<std::vector<int>>> demand_paths;
// demand paths as list of pairs, pair = {node id, output interface}
std::vector<std::vector<std::vector<std::pair<int, int>>>> demand_path_output_interfaces;
// flow id for each path for each demand
std::vector<std::vector<int>> demand_path_flowids;
// number of vertices for each path for each demand 
// (including first node and excluding last one (destination node))
std::vector<std::vector<int>> demand_path_no_vertices;
// initial flow on path (in same units as h(d), e.g. kbps), 
// all paths flow for given demand should sum up to h(d) 
std::vector<std::vector<double>> demand_path_initial_flow;
// number of paths for each demand
std::vector<int> demand_no_paths;
// 
std::vector<std::vector<std::vector<int>>> demand_path_edge_ids;


// inefficient for now: just looking through all demands
std::pair<int, int>
GetDemandPathIdsByFlowId(int flowId)
{
  for(int demandId = 0; demandId < D; ++demandId){
    for(int pathId = 0; pathId < demand_no_paths[demandId]; ++pathId){
      if(demand_path_flowids[demandId][pathId] == flowId){
        return std::make_pair(demandId, pathId);
      }
    }
  }
  NS_LOG_INFO("--- ERROR: Demand/path pair not found for given flowId !!!");
  return std::make_pair(-1, -1);
}


// read demands, paths, routing data
void
ReadDemandPaths(std::string filePath)
{
  NS_LOG_INFO("--- Read demands and paths from: " << filePath);
  std::ifstream fileStream(filePath);
  if(fileStream.is_open()){
    fileStream >> D;
    demand_fr.resize(D);
    demand_to.resize(D);
    demand_volume.resize(D);
    demand_path_flowids.resize(D);
    demand_path_no_vertices.resize(D);
    demand_path_initial_flow.resize(D);
    demand_path_output_interfaces.resize(D);
    demand_no_paths.resize(D);
    demand_path_edge_ids.resize(D);
    for(int demandId = 0; demandId < D; ++demandId){
      int curDemandId;
      int curFromNodeId;
      int curToNodeId;
      double curDemandVolume;
      int curNoPahts;
      fileStream >> curDemandId >> curFromNodeId >> curToNodeId >> curDemandVolume >> curNoPahts;
      demand_fr[demandId] = curFromNodeId;
      demand_to[demandId] = curToNodeId;
      demand_volume[demandId] = curDemandVolume * SCALE;
      demand_no_paths[demandId] = curNoPahts;
      demand_path_flowids[demandId].resize(curNoPahts);
      demand_path_no_vertices[demandId].resize(curNoPahts);
      demand_path_initial_flow[demandId].resize(curNoPahts);
      demand_path_output_interfaces[demandId].resize(curNoPahts);
      demand_path_edge_ids[demandId].resize(curNoPahts);
      //NS_LOG_INFO(demand_no_paths[demandId]);
      for(int pathId = 0; pathId < curNoPahts; ++pathId){
        int curPathId;
        int curFlowId;
        int curNoVerticiesInPath; 
        double curInitialPathFlow;
        fileStream >> curPathId >> curFlowId >> curNoVerticiesInPath >> curInitialPathFlow;
        demand_path_flowids[demandId][pathId] = curFlowId;
        demand_path_no_vertices[demandId][pathId] = curNoVerticiesInPath;
        //demand_path_initial_flow[demandId][pathId] = curInitialPathFlow;
        demand_path_initial_flow[demandId][pathId] = curInitialPathFlow * SCALE;
        demand_path_output_interfaces[demandId][pathId].resize(curNoVerticiesInPath);
        demand_path_edge_ids[demandId][pathId].resize(curNoVerticiesInPath);
        for(int vertexId = 0; vertexId < curNoVerticiesInPath; ++vertexId){
          int curEdgeId;
          int curVertexId;
          int curOutputInterfaceId;
          fileStream >> curEdgeId >> curVertexId >> curOutputInterfaceId;
          // vertexId is just consecutive numbers from 0 to number of nodes in paths
          // curVertexId is actual node id
          demand_path_output_interfaces[demandId][pathId][vertexId] = std::make_pair(curVertexId, curOutputInterfaceId);
          // variable vertexID is ok here, curNoVerticiesInPath = number of edges in path (because we exclude last node)
          demand_path_edge_ids[demandId][pathId][vertexId] = curEdgeId;
        }
      }
    }

    fileStream.close();
  }else{
    NS_LOG_INFO("--- ERROR: DemandPaths FILE NOT OPENED!!!");
  }
  NS_LOG_INFO("--- End read demand and paths");
}
/////////////////////// end demands/paths


/////////////////////// events 
// number of events associated with edge bandwidth changes
int no_edge_events;
// time of edge bandwidth change event; in seconds
std::vector<double> event_edge_time;
// edge id of edge bandwidth change event
std::vector<int> event_edge_id;
// bandwidth value of edge bandwidth change event; 
// same units as in initial state are assumed 
std::vector<double> event_edge_bandwidth;
// number of events associated with onOff app dataRate changes
int no_app_events;
// time of onOff app dataRate change event; in seconds
std::vector<double> event_app_time;
// flowId if onOff app dataRate change event
std::vector<int> event_app_flowid;
// dataRate value onOff app dataRate change event;
// same units as in initial state are assumed 
std::vector<double> event_app_datarate;


// read events and fill appropriate events structeres
void
ReadEvents(std::string filePath)
{
  NS_LOG_INFO("--- Read events from: " << filePath);
  std::ifstream fileStream(filePath);
  if(fileStream.is_open()){
    fileStream >> no_edge_events;
    event_edge_time.resize(no_edge_events);
    event_edge_id.resize(no_edge_events);
    event_edge_bandwidth.resize(no_edge_events);
    for(int edgeEventId = 0; edgeEventId < no_edge_events; ++edgeEventId){
      fileStream >> event_edge_time[edgeEventId];
      fileStream >> event_edge_id[edgeEventId];
      fileStream >> event_edge_bandwidth[edgeEventId];
      event_edge_bandwidth[edgeEventId] *= SCALE * EDGE_BANDWIDTH_MARGIN;
      //NS_LOG_INFO(event_edge_time[edgeEventId] << " " << event_edge_id[edgeEventId] << " " << event_edge_bandwidth[edgeEventId]);
    }

    fileStream >> no_app_events;
    event_app_time.resize(no_app_events);
    event_app_flowid.resize(no_app_events);
    event_app_datarate.resize(no_app_events);
    for(int appEventId = 0; appEventId < no_app_events; ++appEventId){
      fileStream >> event_app_time[appEventId];
      fileStream >> event_app_flowid[appEventId];
      fileStream >> event_app_datarate[appEventId];
      event_app_datarate[appEventId] *= SCALE;
      //NS_LOG_INFO(event_app_time[appEventId] << " " << event_app_flowid[appEventId] << " " << event_app_datarate[appEventId]);
    }

    fileStream.close();
  }else{
    NS_LOG_INFO("--- ERROR: Events FILE NOT OPENED!!!");
  }
  NS_LOG_INFO("--- End read events");
}
/////////////////////// end events


//////////////////////// simulation structures
// ns nodes, access by nodeId
NodeContainer nodes;
// access by edgeId, used in bandwidth change schedulling
std::vector<NetDeviceContainer> netDeviceContainers;
// ipv4 addresses of netDevices
// used in routing and apps creation
std::vector<Ipv4InterfaceContainer> ipv4InterfaceContainers;
// 
FtIpv4StaticRoutingHelper ipv4RoutingHelper;
// 
InternetStackHelper internetStack;
// ptrs to FtOnOffApplications for each demand and path;
// filled in CreateApplications(), used later in events
std::vector<std::vector<Ptr<Application>>> demandPathOnOffApps;
// ptrs to PacketSink apps for each demand and path; 
// filled in CreateApplications(), 
// used later in events and getting stats of recieved bytes
std::vector<std::vector<Ptr<Application>>> demandPathSinkApps;
//////////////////////// end simulation structures


//////////////////////// simulation methods
// create ns nodes, set routing (empty at this moment, later it will be filled) 
// add internet on nodes
void 
CreateNodes()
{
  NS_LOG_INFO("--- Create nodes");
  nodes.Create(V);

  internetStack.SetRoutingHelper(ipv4RoutingHelper);
  internetStack.Install(nodes);
  NS_LOG_INFO("--- End create nodes");
}


// create ns edges, channels and devices, fill netDeviceContainers
void
CreateEdges()
{ 
  NS_LOG_INFO("--- Create edges");
  PointToPointHelper pointToPoint;
  pointToPoint.SetChannelAttribute ("Delay", StringValue (std::to_string(LINK_DELAY) + "s"));
  //pointToPoint.SetQueue("ns3::DropTailQueue", "MaxBytes", UintegerValue(10000));
  pointToPoint.SetQueue("ns3::DropTailQueue", "MaxSize", ns3::QueueSizeValue(QueueSize(std::to_string(QUEUE_SIZE) + "p")));
  
  Ipv4AddressHelper ipv4;

  for(int edgeId = 0; edgeId < E; ++edgeId){
    int nodeFromId = edges[edgeId].first;
    int nodeToId = edges[edgeId].second;

    std::string dataRate = std::to_string(initial_edge_bandwidth[edgeId]) + FLOW_UNIT;
    
    pointToPoint.SetDeviceAttribute ("DataRate", StringValue (dataRate));
    NodeContainer nodeContainer = NodeContainer(nodes.Get(nodeFromId), nodes.Get(nodeToId));
    NetDeviceContainer netDeviceContainer = pointToPoint.Install (nodeContainer);
    netDeviceContainers.emplace_back(netDeviceContainer);
    
    // assign ip addresses/masks to edges
    std::string address = "10.1." + std::to_string(edgeId + 1) + ".0";

    ipv4.SetBase(Ipv4Address(address.c_str()), "255.255.255.252");
    Ipv4InterfaceContainer ipv4InterfaceCont = ipv4.Assign(netDeviceContainer);
    ipv4InterfaceContainers.emplace_back(ipv4InterfaceCont);
  }
  NS_LOG_INFO("--- End create edges");
}


// add all routing table entries based on information from ReadDemandPaths() 
void
CreateRouting()
{
  NS_LOG_INFO("--- Create routing");
  for(int demandId = 0; demandId < D; ++demandId){
    for(int pathId = 0; pathId < demand_no_paths[demandId]; ++pathId){
      int curFlowId = demand_path_flowids[demandId][pathId];
      int curPathLength = demand_path_edge_ids[demandId][pathId].size();
      int curPathLastEdgeId = demand_path_edge_ids[demandId][pathId][curPathLength - 1];
      Ipv4Address curDemandDestinationAddress = (edges[curPathLastEdgeId].first == demand_to[demandId]) ? 
                                                ipv4InterfaceContainers[curPathLastEdgeId].GetAddress(0) :
                                                ipv4InterfaceContainers[curPathLastEdgeId].GetAddress(1);
      NS_LOG_INFO(curFlowId);   
      NS_LOG_INFO(curPathLength);
      NS_LOG_INFO(curPathLastEdgeId);
      NS_LOG_INFO(curDemandDestinationAddress);
      
      //vertexId - only for iteration throw each vertex in current path; 0,1,...,pathSize-1; used also for edge iterating
      //curVertexId - actual nodeId
      for(int vertexId = 0; vertexId < curPathLength; ++vertexId){
        int curVertexId = demand_path_output_interfaces[demandId][pathId][vertexId].first;
        int curOutputInterfaceId = demand_path_output_interfaces[demandId][pathId][vertexId].second;
        int curPathEdgeId = demand_path_edge_ids[demandId][pathId][vertexId];
        // if current nodeId == first node of cur edge, then we need second nodeId as curToAddress
        Ipv4Address curToAddress = (edges[curPathEdgeId].first == curVertexId) ? 
                                                ipv4InterfaceContainers[curPathEdgeId].GetAddress(1) :
                                                ipv4InterfaceContainers[curPathEdgeId].GetAddress(0);

        // ipv4 of current node
        Ptr<Ipv4> curIpv4 = (nodes.Get(curVertexId))->GetObject<Ipv4>();
        // pointer to static routing(routing table) of current node
        Ptr<FtIpv4StaticRouting> curVertexStaticRouting = ipv4RoutingHelper.GetStaticRouting(curIpv4);
        // adding one current entry to routing table
        curVertexStaticRouting->AddHostRouteTo(curFlowId, curDemandDestinationAddress, curToAddress, curOutputInterfaceId);
        NS_LOG_INFO(curPathEdgeId << " " << curVertexId << " " << curOutputInterfaceId);
        NS_LOG_INFO("---adding routing entry:" << curFlowId << " " << curDemandDestinationAddress << " " << curToAddress << " " << curOutputInterfaceId);
      }                                  
    }
  }
  NS_LOG_INFO("--- End create routing");
}


// get ipv4 address, that is assigned to the net device of demandId destination node and
// at the same time belongs to last edge of pathId; used to create traffic generating apps
Ipv4Address
GetDestinationAddress(int demandId, int pathId)
{
  int pathLength = demand_path_no_vertices[demandId][pathId];
  int lastEdgeId = demand_path_edge_ids[demandId][pathId][pathLength - 1];
  Ipv4Address toAddress = (edges[lastEdgeId].first == demand_to[demandId]) ? 
                          ipv4InterfaceContainers[lastEdgeId].GetAddress(0) :
                          ipv4InterfaceContainers[lastEdgeId].GetAddress(1);
  return toAddress; 
}


// convert double datarate value into string with FLOW_UNIT
// handles 0.0 datarate value, setting to very low datarate value 
std::string
GetDataRateString(double dataRate)
{
  // If initialy current dataRate == 0.0 (i.e., path is not used), 
  // set its dataRate to low value ("1bps" for instance).
  // This way next send packet event will be scheduled at time packetSize/dataRate,
  // t = 25B / 1bps = 200s, t should exceed simulation time (increasing the packetSize may be needed).      
  std::string dataRateString = (abs(dataRate) < 0.0000001) ? 
                                "1bps" : 
                                std::to_string(dataRate) + FLOW_UNIT;
  return dataRateString;
}


// add packet generators and sink apps
void
CreateApplications()
{
  NS_LOG_INFO("--- Create applications");
  std::string protocol = "ns3::UdpSocketFactory";
  int temp_port = 9;
  Time onOffStartTime = Seconds(START_SIMULATION_TIME);
  Time onOffStopTime = Seconds(END_APPS_TIME);
  Time sinkStartTime = Seconds(START_SIMULATION_TIME);
  Time sinkStopTime = Seconds(END_SIMULATION_TIME);

  // destination address is dummy, will be changed in demand/paths loops by setting "Remote" attribute
  FtOnOffHelper ftOnOffHelper(protocol, InetSocketAddress(Ipv4Address("14.14.14.14"), temp_port));
  ftOnOffHelper.SetAttribute("PacketSize", UintegerValue(PACKET_SIZE));
  // setting large OnTime and zero OffTime constant so that random variables
  // that are used to determine next on/off period will not influence the simulation
  // 1000 const should be larger total simulation time
  ftOnOffHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1000]"));
  ftOnOffHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));

  //PacketSinkHelper packetSinkHelper(protocol, Address(InetSocketAddress(Ipv4Address("14.14.14.14"), port)));   
  PacketSinkHelper packetSinkHelper(protocol, Address(InetSocketAddress(Ipv4Address::GetAny(), temp_port))); 

  demandPathOnOffApps.resize(D);
  demandPathSinkApps.resize(D);
  for(int demandId = 0; demandId < D; ++demandId){
    demandPathOnOffApps[demandId].resize(demand_no_paths[demandId]);
    demandPathSinkApps[demandId].resize(demand_no_paths[demandId]);
    for(int pathId = 0; pathId < demand_no_paths[demandId]; ++pathId){
      // setting path-specific attributes

      // same port in both OnOff and PacketSink apps
      int port = demand_path_flowids[demandId][pathId];
      Ipv4Address destinationAddress = GetDestinationAddress(demandId, pathId);
      ftOnOffHelper.SetAttribute("Remote", AddressValue(InetSocketAddress(destinationAddress, port)));
      
      std::string dataRateString = GetDataRateString(demand_path_initial_flow[demandId][pathId]);
      DataRate dataRate = DataRate(dataRateString);
      ftOnOffHelper.SetAttribute("DataRate", DataRateValue(dataRate));
      int flowId = demand_path_flowids[demandId][pathId];
      ftOnOffHelper.SetAttribute("FlowId", UintegerValue(flowId));

      ApplicationContainer onOffAppContainer = ftOnOffHelper.Install(nodes.Get(demand_fr[demandId]));
      onOffAppContainer.Start(onOffStartTime);
      onOffAppContainer.Stop(onOffStopTime);      
      //Ptr<FtOnOffApplication> onOffApp = DynamicCast<FtOnOffApplication> (onOffAppContainer.Get(0));
      //onOffApp->ScheduleStartAppOnTime(onOffStartTime);
      //onOffApp->ScheduleStopAppOnTime(onOffStopTime);

      // current onOff app has been installed only on one node (i.e. demands source node), 
      // onOffAppContainer has only one app, get here 0th app 
      demandPathOnOffApps[demandId][pathId] = onOffAppContainer.Get(0);

      // sink app
      packetSinkHelper.SetAttribute("Local", AddressValue(InetSocketAddress(destinationAddress, port)));
      ApplicationContainer sinkAppContainer = packetSinkHelper.Install(nodes.Get(demand_to[demandId]));
      sinkAppContainer.Start(sinkStartTime);
      sinkAppContainer.Stop(sinkStopTime);
      demandPathSinkApps[demandId][pathId] = sinkAppContainer.Get(0);
    }
  }
  NS_LOG_INFO("--- End create applications");
}


// change datarate of the edge in both directions
void 
ChangeBandwidth(const StringValue & dataRateValue, NetDeviceContainer & devices)
{
  devices.Get(0)->SetAttribute("DataRate", dataRateValue);
  devices.Get(1)->SetAttribute("DataRate", dataRateValue);

  //StringValue dr0;
  //devices.Get(0)->GetAttribute("DataRate", dr0);
  //NS_LOG_INFO("-------after datarate 0 " << dr0.Get());
  //StringValue dr1;
  //devices.Get(1)->GetAttribute("DataRate", dr1);
  //NS_LOG_INFO("-------after datarate 1 " << dr1.Get());
}


// schedule link datarate change
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


// schedule traffic generator datarate
void 
ScheduleTrafficRateChange(const Time & scheduleTime, const StringValue & trafficRateValue, Ptr<Application> application)
{
  Simulator::Schedule (scheduleTime, &ChangeTrafficRate, trafficRateValue, application, scheduleTime); 
}


// add events to the simulator (schedule) 
// edge bandwidth changes and onOff app datarate changes 
void
CreateEvents()
{
  NS_LOG_INFO("--- Create events");
  for(int edgeEventId = 0; edgeEventId < no_edge_events; ++edgeEventId){
    ScheduleDataRateChange(Seconds(event_edge_time[edgeEventId]), 
                           StringValue(std::to_string(event_edge_bandwidth[edgeEventId]) + FLOW_UNIT), 
                           netDeviceContainers[event_edge_id[edgeEventId]]);
  }

  for(int appEventId = 0; appEventId < no_app_events; ++appEventId){
    std::pair<int, int> demandPathIds = GetDemandPathIdsByFlowId(event_app_flowid[appEventId]);
    int demandId = demandPathIds.first;
    int pathId = demandPathIds.second;
    std::string dataRateString = GetDataRateString(event_app_datarate[appEventId]);
    //std::string dataRateString = GetDataRateString(0.9);
    ScheduleTrafficRateChange(Seconds(event_app_time[appEventId]),
                              StringValue(dataRateString),
                              demandPathOnOffApps[demandId][pathId]);
  }
  NS_LOG_INFO("--- End create events");
}


//////////////////////// saving flow stats
// for each demand and path store results samples
// sample = sim time + bytes sent + bytes recieved
std::vector<std::vector<std::string>> result_flow_samples;


// for given demandId and pathId get full string of file paths used in ofstreams 
std::string
GetFlowOutputFilePath(std::string & dataPath, int & demandId, int & pathId)
{
  return dataPath + "res/d" + std::to_string(demandId) + "p" + std::to_string(pathId) + ".txt";
}


void 
SaveFlowSentRecievedBytes(int & demandId, int & pathId)
{
  Ptr<PacketSink> packetSinkApp = DynamicCast<PacketSink> (demandPathSinkApps[demandId][pathId]);
  uint64_t totalRecieved = packetSinkApp->GetTotalRx();
  
  Ptr<FtOnOffApplication> onOffApp = DynamicCast<FtOnOffApplication> (demandPathOnOffApps[demandId][pathId]);
  uint64_t totalSent = onOffApp->GetTotalSent();

  std::string sample = std::to_string(Simulator::Now().GetSeconds()) + " " + 
                                      std::to_string(totalSent) + " " + 
                                      std::to_string(totalRecieved) + "\n";
  result_flow_samples[demandId][pathId] += sample;
  //NS_LOG_INFO("-------sent/recv bytes sample = " << sample);

  // show some queue stats
  /*
  for(int edgeId = 0; edgeId < E; ++edgeId){
    Ptr<PointToPointNetDevice> pp0 = DynamicCast<PointToPointNetDevice> (netDeviceContainers[edgeId].Get(0));
    Ptr<PointToPointNetDevice> pp1 = DynamicCast<PointToPointNetDevice> (netDeviceContainers[edgeId].Get(1));
   	uint32_t d0 = pp0->GetQueue()->GetNPackets();
    uint32_t d1 = pp1->GetQueue()->GetNPackets();
    NS_LOG_INFO("-------queue no packets = " << d0 << " " << d1);
    NS_LOG_INFO("-------queue no dropped = " << pp0->GetQueue()->GetTotalDroppedPackets() << " " << pp0->GetQueue()->GetTotalDroppedPackets());
    NS_LOG_INFO("-------queue current size = " << pp0->GetQueue()->GetCurrentSize());
    NS_LOG_INFO("-------queue max size = " << pp0->GetQueue()->GetMaxSize());
  }
  */
}


// schedule getting flow stats
void 
ScheduleSavingFlowSentRecievedBytes(const Time & scheduleTime, int & demandId, int & pathId)
{
  Simulator::Schedule (scheduleTime, &SaveFlowSentRecievedBytes, demandId, pathId);
}


// schedule taking flow result samples every STATS_DELTA_TIME
void
SaveFlowStatistics(std::string dataPath)
{
  result_flow_samples.resize(D);
  for(int demandId = 0; demandId < D; ++demandId){
    result_flow_samples[demandId].resize(demand_no_paths[demandId]);
    for(int pathId = 0; pathId < demand_no_paths[demandId]; ++pathId){
      result_flow_samples[demandId][pathId] = "";
      for(double time = 0.0; time < END_SIMULATION_TIME; time += STATS_DELTA_TIME){
        ScheduleSavingFlowSentRecievedBytes(Seconds(time), demandId, pathId);
      }
    }
  } 
}


// save result flow string samples to files
// call after simulation ends
void
SaveFlowResultStringsToFiles(std::string & dataPath)
{
  for(int demandId = 0; demandId < D; ++demandId){
    for(int pathId = 0; pathId < demand_no_paths[demandId]; ++pathId){
      std::string curFilePath = GetFlowOutputFilePath(dataPath, demandId, pathId);
      std::ofstream outputFile(curFilePath);
      //NS_LOG_INFO(result_flow_samples[demandId][pathId]);
      outputFile << "time sent recieved\n";
      outputFile << result_flow_samples[demandId][pathId];
      outputFile.close();
    }
  }
}


//////////////////////// saving queue stats
// for each queue (edgeId + nodeId, i.e from which side of edge) store results sample
// sample = sim time + current no packets in queue
std::vector<std::vector<std::string>> result_queue_samples;


// for given edgeId and nodeID get full string of file paths used in ofstreams 
std::string
GetQueueOutputFilePath(std::string & dataPath, int & edgeId, int & nodeId)
{
  return dataPath + "res/e" + std::to_string(edgeId) + "n" + std::to_string(nodeId) + ".txt";
}


// adds single sample of current number of packets in edge queue (from both sides)
void 
SaveQueueNoPackets(int & edgeId)
{  
  Ptr<PointToPointNetDevice> pp0 = DynamicCast<PointToPointNetDevice> (netDeviceContainers[edgeId].Get(0));
  uint32_t noPackets0 = pp0->GetQueue()->GetNPackets();
  Ptr<PointToPointNetDevice> pp1 = DynamicCast<PointToPointNetDevice> (netDeviceContainers[edgeId].Get(1));
  uint32_t noPackets1 = pp1->GetQueue()->GetNPackets();

  std::string sample0 = std::to_string(Simulator::Now().GetSeconds()) + " " + 
                                       std::to_string(noPackets0) + "\n";
  result_queue_samples[edgeId][0] += sample0;

  std::string sample1 = std::to_string(Simulator::Now().GetSeconds()) + " " + 
                                       std::to_string(noPackets1) + "\n";
  result_queue_samples[edgeId][1] += sample1;

  NS_LOG_INFO("-------queue no packets = " << sample0 << " " << sample1);
  NS_LOG_INFO("-------queue max size   = " << pp0->GetQueue()->GetMaxSize() << " " << pp1->GetQueue()->GetMaxSize());
  NS_LOG_INFO("-------queue cur size   = " << pp0->GetQueue()->GetCurrentSize() << " " << pp1->GetQueue()->GetCurrentSize());
  NS_LOG_INFO("-------queue no dropped = " << pp0->GetQueue()->GetTotalDroppedPackets() << " " << pp0->GetQueue()->GetTotalDroppedPackets());
}


// schedule getting queue stats
void 
ScheduleSavingQueueNoPackets(const Time & scheduleTime, int & edgeId)
{
  Simulator::Schedule (scheduleTime, &SaveQueueNoPackets, edgeId);
}


// schedule taking queue state result samples every STATS_DELTA_TIME
void
SaveQueueStatistics(std::string dataPath)
{
  //NS_LOG_INFO("------- save queue statistics method");
  result_queue_samples.resize(E);
  for(int edgeId = 0; edgeId < E; ++edgeId){
    result_queue_samples[edgeId].resize(2);
    result_queue_samples[edgeId][0] = "";
    result_queue_samples[edgeId][1] = "";
    for(double time = 0.0; time < END_SIMULATION_TIME; time += STATS_DELTA_TIME){
      ScheduleSavingQueueNoPackets(Seconds(time), edgeId);
    }
  } 
  //NS_LOG_INFO("------- end save queue statistics method");
}


// save result queue string samples to files
// call after simulation ends
// variable edgesNode - 0th or 1th node of edge
void
SaveQueueResultStringsToFiles(std::string & dataPath)
{
  for(int edgeId = 0; edgeId < E; ++edgeId){
    for(int edgesNode = 0; edgesNode < 2; ++edgesNode){
      int nodeId = edgesNode == 0 ? edges[edgeId].first : edges[edgeId].second;
      std::string curFilePath = GetQueueOutputFilePath(dataPath, edgeId, nodeId);
      std::ofstream outputFile(curFilePath);
      outputFile << "time noPackets\n";
      outputFile << result_queue_samples[edgeId][edgesNode];
      outputFile.close();
    }
  }
}

// enable chosen logs to show in the console
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
  //LogComponentEnable ("Queue", LOG_LEVEL_ALL);
}


void
runSumulation()
{
  double start_time = get_time();

  std::string const HOME = std::getenv("HOME") ? std::getenv("HOME") : ".";
  std::string dataPath = HOME + "/Desktop/simdata/example-report/";

  std::string paramsFilePath = dataPath + "params.txt";
  std::string graphFilePath = dataPath + "graph.txt";
  std::string routingFilePath = dataPath + "routing.txt";
  std::string eventsFilePath = dataPath + "events.txt";

  ReadParams(paramsFilePath);

  ReadGraph(graphFilePath);
  CreateNodes();
  CreateEdges();
  
  ReadDemandPaths(routingFilePath);
  CreateRouting();
  CreateApplications();
  
  ReadEvents(eventsFilePath);
  CreateEvents();
  SaveFlowStatistics(dataPath);
  SaveQueueStatistics(dataPath);
  
  /*  
  // test: print nodes and output interafeces for all paths
  for(int demandId = 0; demandId < D; ++demandId){
    for(int pathId = 0; pathId < demand_no_paths[demandId]; ++pathId){
      for(int nodeId = 0; nodeId < demand_path_no_vertices[demandId][pathId]; ++nodeId){
        NS_LOG_INFO(demand_path_output_interfaces[demandId][pathId][nodeId].first << " "
                    << demand_path_output_interfaces[demandId][pathId][nodeId].second);
      }
    }
  }
  */

    //run simulation
  Simulator::Stop(Seconds(END_SIMULATION_TIME));
  double init_time = get_time() - start_time;
  
  NS_LOG_INFO("\n################ START SIMULATION ################\n");
  Simulator::Run ();
  NS_LOG_INFO("\n################ END SIMULATION ################\n");
  
  double total_time = get_time() - start_time;
  NS_LOG_INFO("--- ini time = " + std::to_string(init_time) + "s");
  NS_LOG_INFO("--- sim time = " + std::to_string(total_time - init_time) + "s");
  NS_LOG_INFO("--- tot time = " + std::to_string(total_time) + "s");
  
  Simulator::Destroy ();
  SaveFlowResultStringsToFiles(dataPath);
  SaveQueueResultStringsToFiles(dataPath);
}


int
main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);
  Time::SetResolution (Time::NS);

  EnableLogComponents();
  runSumulation();

  return 0;
}
