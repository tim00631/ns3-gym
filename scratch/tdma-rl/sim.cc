/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2011 Hemanth Narra
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
 * Author: Hemanth Narra <hemanthnarra222@gmail.com>
 *
 * James P.G. Sterbenz <jpgs@ittc.ku.edu>, director
 * ResiliNets Research Group  http://wiki.ittc.ku.edu/resilinets
 * Information and Telecommunication Technology Center (ITTC)
 * and Department of Electrical Engineering and Computer Science
 * The University of Kansas Lawrence, KS USA.
 *
 * Work supported in part by NSF FIND (Future Internet Design) Program
 * under grant CNS-0626918 (Postmodern Internet Architecture),
 * NSF grant CNS-1050226 (Multilayer Network Resilience Analysis and Experimentation on GENI),
 * US Department of Defense (DoD), and ITTC at The University of Kansas.
 */
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/simple-wireless-tdma-module.h"
#include "ns3/dsdv-module.h"
#include "ns3/olsr-helper.h"
#include "ns3/mobility-model.h"
#include "ns3/llc-snap-header.h"
#include "ns3/olsr-routing-protocol.h"
#include "ns3/object.h"

#include "ns3/opengym-module.h"
#include "tdma-rl-env.h"

#include <iostream>
#include <cmath>
#include <string>

using namespace ns3;

uint64_t recvBytes = 0;

uint16_t port = 8080;
uint32_t openGymPort = 5555;

NS_LOG_COMPONENT_DEFINE ("TdmaExample");

class TdmaExample
{
public:
  TdmaExample ();
  void CaseRun (uint32_t nWifis,
		uint32_t Sink,
                double totalTime,
                std::string rate,
                uint32_t settlingTime,
                double dataStart,
		bool selfGenerate,
                double txpDistance,
                uint32_t slotTime,
                uint32_t guardTime,
                uint32_t interFrameGap,
		uint32_t pktNum,
		double pktInterval);
  
  //static uint64_t GetBytesTotal();
    
private:
  uint32_t m_nWifis;
  uint32_t m_Sink;
  double m_totalTime;
  std::string m_rate;
  uint32_t m_settlingTime;
  double m_dataStart;
  //static uint64_t bytesTotal;
  uint32_t packetsReceived;
  uint32_t m_slotTime;
  uint32_t m_guardTime;
  uint32_t m_interFrameGap;
  uint32_t m_pktNum;
  double m_pktInterval;
  //std::vector<std::map<Ipv4Address,Ptr<Socket>>> m_socketMap; 
  std::vector<Ptr<Socket>> m_socketMap; 

  std::map<double, double> m_transmitRangeMap;

  NodeContainer nodes;
  NetDeviceContainer devices;
  Ipv4InterfaceContainer interfaces;
  

private:
  void CreateNodes ();
  void CreateDevices (double txpDistance);
  void InstallInternetStack ();
  void InstallApplications (bool selfGenerate);
  void SetupMobility ();
  void ReceivePacket (Ptr <Socket> );
  Ptr <Socket> SetupPacketReceive (Ipv4Address, Ptr <Node> );
  void CheckThroughput ();
  void GenerateTraffic (uint32_t nodeId, uint32_t pktSize,
                             uint32_t remainTransmit, Time pktInterval , uint32_t pktNum);
};


int main (int argc, char **argv)
{
  Packet::EnablePrinting ();

  TdmaExample test;
  uint32_t nWifis = 16;
  uint32_t Sink = 3;
  double totalTime = 300.0;
  std::string rate ("8kbps");
  std::string appl = "all";
  uint32_t settlingTime = 6;
  double dataStart = 5.0;
  double txpDistance = 1000.0;
  bool selfGenerate = true;

  // tdma parameters
  // slotTime is at least the number of bytes in a packet * 8 bits/byte / bit rate * 1e6 microseconds
  uint32_t slotTime = 1000 * 8 / 8000 * 1000000; // us
  uint32_t interFrameGap = 0;
  uint32_t guardTime = 0;
  uint32_t pktNum = 20;
  double pktInterval = 0.04;
  uint32_t simSeed = 0;
  //srand(30000);

  CommandLine cmd;
  cmd.AddValue ("nWifis", "Number of wifi nodes[Default:30]", nWifis);
  cmd.AddValue ("Sink", "Set Sink node", Sink);
  cmd.AddValue ("selfGenerate", "Do you want to generate traffic by ourselves[Default:true]",selfGenerate);
  cmd.AddValue ("totalTime", "Total Simulation time[Default:100]", totalTime);
  cmd.AddValue ("rate", "CBR traffic rate[Default:8kbps]", rate);
  cmd.AddValue ("settlingTime", "Settling Time before sending out an update for changed metric[Default=6]", settlingTime);
  cmd.AddValue ("dataStart", "Time at which nodes start to transmit data[Default=50.0]", dataStart);
  cmd.AddValue ("txpDistance", "MaxRange for the node transmissions [Default:400.0]", txpDistance);
  cmd.AddValue ("slotTime", "Slot transmission Time [Default(us):1000]", slotTime);
  cmd.AddValue ("guardTime", "Duration to wait between slots [Default(us):0]", guardTime);
  cmd.AddValue ("interFrameGap", "Duration between frames [Default(us):0]", interFrameGap);
  cmd.AddValue ("pktNum","Number of Packet in each transmission",pktNum);
  cmd.AddValue ("pktInterval","Time between two packet stream",pktInterval);
  cmd.AddValue ("openGymPort", "OpenGymPort", openGymPort);
  cmd.AddValue ("simSeed", "simSeed", simSeed);
  cmd.Parse (argc, argv);
  
  RngSeedManager::SetSeed (1);
  RngSeedManager::SetRun (388119029);
  //RngSeedManager::SetRun (simSeed);

  Config::SetDefault ("ns3::OnOffApplication::PacketSize", StringValue ("1000")); // bytes!
  Config::SetDefault ("ns3::OnOffApplication::DataRate", StringValue (rate));

  LogComponentEnable ("TdmaExample", LOG_LEVEL_DEBUG);

  test = TdmaExample ();
  test.CaseRun (nWifis, Sink, totalTime, rate, 
                settlingTime, dataStart,selfGenerate,txpDistance, slotTime, guardTime, interFrameGap, pktNum, pktInterval);

  return 0;
}

TdmaExample::TdmaExample ()
  : m_nWifis(30),
		m_Sink(1),
		m_totalTime(300),
	  m_rate ("8kbps"),
		m_settlingTime (6),
	  m_dataStart (5.0),
    packetsReceived (0),
	  m_slotTime (1000),
    m_guardTime (0),
	  m_interFrameGap (0),
	  m_pktNum(1),
	  m_pktInterval(1.0)

{
  NS_LOG_FUNCTION (this);
}


void 
TdmaExample::GenerateTraffic (uint32_t nodeId, uint32_t pktSize,
                             uint32_t remainTransmit, Time pktInterval , uint32_t pktNum)
{
  std::ostringstream oss;
  oss <<"/NodeList/"<<nodeId<<"/$ns3::olsr::RoutingProtocol";
  Config::MatchContainer m = Config::LookupMatches(oss.str());
  std::vector<ns3::olsr::RoutingTableEntry> tdmaRoutingTable = m.Get(0)->GetObject<ns3::olsr::RoutingProtocol>()->GetRoutingTableEntries();
  
  /*
  Ptr<Node> node = NodeList::GetNode (nodeId);
  std::vector<ns3::olsr::RoutingTableEntry> tdmaRoutingTable = node->GetObject<ns3::olsr::RoutingProtocol> ()->GetRoutingTableEntries();
  */
         
                                                   
  NS_LOG_UNCOND("Routing table: Node:" << nodeId);
  for(auto rule:tdmaRoutingTable){
         NS_LOG_UNCOND(rule.destAddr);
  }
  
  //Ptr<UniformRandomVariable> rng = CreateObject<UniformRandomVariable> ();
  //rng->SetAttribute ("Min",DoubleValue (0));
  //rng->SetAttribute ("Max",DoubleValue (tdmaRoutingTable.size()-1));
  
  //uint32_t rngVal = rng->GetValue();
  //NS_LOG_UNCOND("rngVal:"<<rngVal);
  //std::random_shuffle (tdmaRoutingTable.begin(),tdmaRoutingTable.end());
  
    
  if (remainTransmit > 0)
  {
	std::ostringstream msg; msg << "Hello World!" << '\0';
	Ptr<Packet> packet;
   
	for (uint32_t i=0;i<pktNum;i++)
	{
		packet = Create<Packet> ((uint8_t*) msg.str().c_str(), pktSize);
		m_socketMap[nodeId]->Send (packet);
	}
	
    	Simulator::Schedule (pktInterval, &TdmaExample::GenerateTraffic, this,
               	           nodeId, pktSize,remainTransmit - 1, pktInterval, pktNum);

  }
  else if (remainTransmit <= 0)
  {
	m_socketMap[nodeId]->Close();
        
  }
  /*
  
  
  if (tdmaRoutingTable.size() != 0)
  {
  	//std::map<Ipv4Address,Ptr<Socket>>::iterator it = m_socketMap[nodeId].find(tdmaRoutingTable[rngVal].destAddr);
  	std::map<Ipv4Address,Ptr<Socket>>::iterator it = m_socketMap[nodeId].find(tdmaRoutingTable[0].destAddr);
	if (it != m_socketMap[nodeId].end() && remainTransmit > 0)
	{
		//std::ostringstream msg; msg << "Hello World!" << '\0';
		//Ptr<Packet> packet;
   
		for (uint32_t i=0;i<pktNum;i++)
		{
			//packet = Create<Packet> ((uint8_t*) msg.str().c_str(), pktSize);
			//it->second->Send (packet);
		}
	
      		Simulator::Schedule (pktInterval, &TdmaExample::GenerateTraffic, this,
                	           nodeId, pktSize,remainTransmit - 1, pktInterval, pktNum);

	}
	else if (remainTransmit <= 0)
	{
		it->second->Close();
        
	}
  }
  
  */
}


// ReceivePacket Callback function
void
TdmaExample::ReceivePacket (Ptr <Socket> socket)
{
  std::cout <<"Time: "<<Simulator::Now ().GetNanoSeconds () << "ns, Node "<< socket->GetNode()->GetId() << " Received one packet!" << std::endl;
  NS_LOG_UNCOND("Time: "<<Simulator::Now ().GetNanoSeconds () << "ns, Node "<< socket->GetNode()->GetId() << " Received one packet!");
  Ptr <Packet> packet;
  while ((packet = socket->Recv ()))
  {
      	recvBytes += packet->GetSize ();
      	packetsReceived += 1;
    }

  }

// Check node throughput per Seconds (not important)
void
TdmaExample::CheckThroughput ()
{
  double kbs = (recvBytes * 8.0) / 1000;
  //recvBytes = 0;


  std::cout << "Time : " << (Simulator::Now ()).GetSeconds () << ", kbs : " << kbs << ", packetsReceived :" << packetsReceived << ", Sink_node : " << m_Sink << std::endl;

  packetsReceived = 0;
  Simulator::Schedule (Seconds (1.0), &TdmaExample::CheckThroughput, this);
}

// Set ReceivePacket Callback function on specific node
Ptr <Socket>
TdmaExample::SetupPacketReceive (Ipv4Address addr, Ptr <Node> node)
{

  TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
  Ptr <Socket> sink = Socket::CreateSocket (node, tid);
  InetSocketAddress local = InetSocketAddress (addr, port);
  sink->Bind (local);
  sink->SetRecvCallback (MakeCallback ( &TdmaExample::ReceivePacket, this));

  return sink;
}



void
TdmaExample::CaseRun (uint32_t nWifis, uint32_t Sink, double totalTime, std::string rate,
                      uint32_t settlingTime, double dataStart, bool selfGenerate, double txpDistance,
                      uint32_t slotTime, uint32_t guardTime, uint32_t interFrameGap, uint32_t pktNum, double pktInterval)
{
  m_nWifis = nWifis;
  m_Sink = Sink;
  m_totalTime = totalTime;
  m_rate = rate;
  m_settlingTime = settlingTime;
  m_dataStart = dataStart;
  m_slotTime = slotTime;
  m_guardTime = guardTime;
  m_interFrameGap = interFrameGap;
  m_pktNum = pktNum;
  m_pktInterval = pktInterval;

    
  CreateNodes ();
  CreateDevices (txpDistance);
  SetupMobility ();
  InstallInternetStack ();

  InstallApplications (selfGenerate);

    
  Ptr<OpenGymInterface> openGymInterface = CreateObject<OpenGymInterface> (openGymPort);
  Ptr<TdmaGymEnv> tdmaGymEnv = CreateObject<TdmaGymEnv> ();
  tdmaGymEnv->SetOpenGymInterface(openGymInterface);


  std::cout << "\nStarting simulation for " << m_totalTime << " s ...\n";

  //CheckThroughput ();

  Simulator::Stop (Seconds (m_totalTime));
  Simulator::Run ();

  openGymInterface->NotifySimulationEnd();
  Simulator::Destroy ();
}

void
TdmaExample::CreateNodes ()
{
  std::cout << "Creating " << (unsigned) m_nWifis << " nodes.\n";
  nodes.Create (m_nWifis);
}

void
TdmaExample::SetupMobility ()
{
  MobilityHelper mobility;

 // Random initial position
  ObjectFactory pos;
  pos.SetTypeId ("ns3::RandomRectanglePositionAllocator");
  pos.Set ("X", StringValue ("ns3::UniformRandomVariable[Min=500.0|Max=2500.0]"));
  pos.Set ("Y", StringValue ("ns3::UniformRandomVariable[Min=500.0|Max=2500.0]"));

  Ptr<PositionAllocator> positionAlloc = pos.Create ()->GetObject<PositionAllocator> ();  
  mobility.SetPositionAllocator (positionAlloc);

  
  // Set Random walk model
  mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
		  "Mode", StringValue ("Time"),
//		  "Time", StringValue ("10s"), // Change direction per 10s
		  "Speed", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=5.0]"), // Set speed = 5m/s
  		  "Bounds", RectangleValue (Rectangle (0, 3000, 0, 3000))); // walk boundary
  
  mobility.Install (nodes);

  pos.Set ("X", StringValue ("ns3::ConstantRandomVariable[Constant=1500.0]"));
  pos.Set ("Y", StringValue ("ns3::ConstantRandomVariable[Constant=1500.0]"));

  Ptr<PositionAllocator> positionAlloc2 = pos.Create ()->GetObject<PositionAllocator> ();  
  mobility.SetPositionAllocator (positionAlloc2);
 
  mobility.Install (nodes.Get(0));

  //AsciiTraceHelper ascii;
  //MobilityHelper::EnableAsciiAll (ascii.CreateFileStream ("/test/mobility-trace-example.mob"));
}

void
TdmaExample::CreateDevices (double txpDistance)
{
  Config::SetDefault ("ns3::SimpleWirelessChannel::MaxRange", DoubleValue (txpDistance));
      
  TdmaHelper tdma = TdmaHelper ("scratch/tdma-rl/tdmaSlots.txt");
      
  TdmaControllerHelper controller;
  controller.Set ("SlotTime", TimeValue (MicroSeconds (1000)));
  controller.Set ("CtrlSlotTime", TimeValue (MicroSeconds (500)));
  controller.Set ("GuardTime", TimeValue (MicroSeconds (0)));
  controller.Set ("InterFrameTime", TimeValue (MicroSeconds (0)));
  tdma.SetTdmaControllerHelper (controller);
  devices = tdma.Install (nodes);

}

void
TdmaExample::InstallInternetStack ()
{
  OlsrHelper olsr;
  Ipv4StaticRoutingHelper staticRouting;

  Ipv4ListRoutingHelper list;
  list.Add (staticRouting, 0);
  list.Add (olsr, 100);

  InternetStackHelper internet;
  internet.SetRoutingHelper (list); // has effect on the next Install ()
  internet.Install (nodes);  

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  interfaces = address.Assign (devices);
}

void
TdmaExample::InstallApplications (bool selfGenerate)
{
  if (selfGenerate)
  {
      
    for (uint32_t i=0;i<m_nWifis;i++)
    {
        Ptr<Node> node = NodeList::GetNode (i);
		Ptr<Socket> sink = SetupPacketReceive (Ipv4Address::GetAny(),node);
    }
      
      
	for (uint32_t i=0;i<m_nWifis;i++)
	{
        /*
		Ptr<Node> node = NodeList::GetNode (i);
		Ptr<Socket> sink = SetupPacketReceive (Ipv4Address::GetAny(),node);
		std::map<Ipv4Address,Ptr<Socket>> ip2socket;

		for(uint32_t j=0;j<m_nWifis;j++)
		{
			if(i == j) continue;
			
			TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
  			Ptr<Socket> source = Socket::CreateSocket (nodes.Get (j), tid);
		  	InetSocketAddress remote = InetSocketAddress (interfaces.GetAddress (i, 0), port);
		  	source->Connect (remote);
			
			ip2socket.insert(std::make_pair(interfaces.GetAddress (j, 0),source));	
			
		}
        */
        
		
        Ptr<UniformRandomVariable> rng = CreateObject<UniformRandomVariable> ();
        rng->SetAttribute ("Min",DoubleValue (0));
        rng->SetAttribute ("Max",DoubleValue (m_nWifis-1));

        uint32_t rngVal = rng->GetValue();
        while(i == rngVal) rngVal = rng->GetValue();
        
        TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
  		Ptr<Socket> source = Socket::CreateSocket (nodes.Get (i), tid);
		InetSocketAddress remote = InetSocketAddress (interfaces.GetAddress (rngVal, 0), port);
		source->Connect (remote);
        
        m_socketMap.push_back(source);
        
        /*
		for(uint32_t j=0;j<m_nWifis;j++)
		{
			if(i == j) continue;
			
			TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
  			Ptr<Socket> source = Socket::CreateSocket (nodes.Get (i), tid);
		  	InetSocketAddress remote = InetSocketAddress (interfaces.GetAddress (j, 0), port);
		  	source->Connect (remote);
			
			ip2socket.insert(std::make_pair(interfaces.GetAddress (j, 0),source));	
			
		}
        
        m_socketMap.push_back(ip2socket);
        */
        
		

        //Ptr<Node> node_source = NodeList::GetNode (i);
		Simulator::Schedule (Seconds (m_dataStart), &TdmaExample::GenerateTraffic, this,
					i, 512, (m_totalTime-m_dataStart)/m_pktInterval, Seconds(m_pktInterval), m_pktNum);
	}

  }
  else
  {
      for (uint32_t i=0;i<m_nWifis;i++)
      {
        Ptr<Node> node = NodeList::GetNode (i);
		Ptr<Socket> sink = SetupPacketReceive (Ipv4Address::GetAny(),node);
      }
      
      
      Ptr<UniformRandomVariable> rng = CreateObject<UniformRandomVariable> ();
      rng->SetAttribute ("Min",DoubleValue (0));
      rng->SetAttribute ("Max",DoubleValue (m_nWifis-1));
      uint32_t rngVal;
      
      for(uint32_t i=0;i<m_nWifis;i++) {
          
          rngVal = rng->GetValue();
          while(i == rngVal) rngVal = rng->GetValue();
          

          // Set Sink node and PacketSize 
          OnOffHelper onoff ("ns3::UdpSocketFactory", Address (InetSocketAddress (interfaces.GetAddress (rngVal), port)));
          onoff.SetAttribute ("PacketSize", UintegerValue(1024));
          onoff.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=0.004]"));
          onoff.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0.0]"));
          onoff.SetConstantRate (DataRate ("10Mb/s"));

          // Set Source node and application (node i)
          ApplicationContainer apps = onoff.Install (nodes.Get (i));
          apps.Start (Seconds (m_dataStart));
          apps.Stop (Seconds (m_totalTime));
      }
	

  }

}


