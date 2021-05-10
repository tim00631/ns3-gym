#include "tdma-rl-env.h"
#include <sstream>

#include <iostream>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <algorithm>

namespace ns3 {


NS_LOG_COMPONENT_DEFINE ("TdmaGymEnv");

NS_OBJECT_ENSURE_REGISTERED (TdmaGymEnv);

//uint64_t recvBytes = 1;

TdmaGymEnv::TdmaGymEnv ()
{
  NS_LOG_FUNCTION (this);
  m_slotNum = 0;
  m_stepInterval1 = MicroSeconds(500);
  m_stepInterval2 = MicroSeconds(1000*32 + 500);
  m_repeatChoose = 0;

  Simulator::Schedule (NanoSeconds(10.0), &TdmaGymEnv::ScheduleNextStateRead, this);

}

TdmaGymEnv::TdmaGymEnv (Time stepInterval1, Time stepInterval2)
{
  NS_LOG_FUNCTION (this);
  m_slotNum = 0;
  m_stepInterval1 = stepInterval1;
  m_stepInterval2 = stepInterval2;
  m_repeatChoose = 0;

  Simulator::Schedule (NanoSeconds(10.0), &TdmaGymEnv::ScheduleNextStateRead, this);

}

void
TdmaGymEnv::ScheduleNextStateRead ()
{
  NS_LOG_FUNCTION (this);
  if (m_slotNum <15){
  	Simulator::Schedule (m_stepInterval1, &TdmaGymEnv::ScheduleNextStateRead, this);
  }
  else {
  	Simulator::Schedule (m_stepInterval2, &TdmaGymEnv::ScheduleNextStateRead, this);
  }
  
  Notify();
  
  m_slotNum = m_slotNum >= 15 ? 0 : m_slotNum+1;
}

TdmaGymEnv::~TdmaGymEnv ()
{
  NS_LOG_FUNCTION (this);
}

TypeId
TdmaGymEnv::GetTypeId (void)
{
  static TypeId tid = TypeId ("TdmaGymEnv")
    .SetParent<OpenGymEnv> ()
    .SetGroupName ("OpenGym")
    .AddConstructor<TdmaGymEnv> ()
  ;
  return tid;
}

void
TdmaGymEnv::DoDispose ()
{
  NS_LOG_FUNCTION (this);
}

/*
Define action space
*/
Ptr<OpenGymSpace>
TdmaGymEnv::GetActionSpace()
{
  // output : Data slot number
  uint32_t slotRange = 32; // the range of slot number the node could choose

  float low = 0;
  float high = 31;
  std::vector<uint32_t> shape = {slotRange,};
  std::string dtype = TypeNameGet<int32_t> ();

  Ptr<OpenGymBoxSpace> box = CreateObject<OpenGymBoxSpace> (low, high, shape, dtype);
  NS_LOG_UNCOND ("TdmaGetActionSpace: "<<box);
  return box;
}

/*
Define observation space
*/
Ptr<OpenGymSpace>
TdmaGymEnv::GetObservationSpace()
{
  NS_LOG_FUNCTION (this);
  // input :
  // Slot Used Table (size : 32 slots)
  // tdma queue top 3 packet bytes 
  uint32_t dataSlotNum = 32;

  float low = 0;
  float high = 4;
  std::vector<uint32_t> shape = {dataSlotNum, };
  std::string dtype = TypeNameGet<int32_t> ();

  Ptr<OpenGymBoxSpace> slotUsedTable_box = CreateObject<OpenGymBoxSpace> (low, high, shape, dtype);

  uint32_t topNkinds = 3;

  float low2 = 0;
  float high2 = 2000;
  std::vector<uint32_t> shape2 = {topNkinds+1, };
  std::string dtype2 = TypeNameGet<uint32_t> ();

  Ptr<OpenGymBoxSpace> pktBytes_box = CreateObject<OpenGymBoxSpace> (low2, high2, shape2, dtype2);

  Ptr<OpenGymDictSpace> space = CreateObject<OpenGymDictSpace> ();
  space->Add("slotUsedTable", slotUsedTable_box);
  space->Add("pktBytes", pktBytes_box);
 

  NS_LOG_UNCOND ("TdmaGetObservationSpace"<<space);

  return space;

}


/*
Define game over condition
*/
bool
TdmaGymEnv::GetGameOver()
{
  NS_LOG_FUNCTION (this);

  
  bool isGameOver = false;
  
  // Time up
  if (Simulator::Now ().GetSeconds () > Seconds(300))
  {
      isGameOver = true;
  } 
  
/*    
  // Reward < -300
  Ptr<Node> node = NodeList::GetNode (m_slotNum);
  Ptr<NetDevice> dev = node-> GetDevice(0);
  Ptr<TdmaNetDevice> m_tdmaDevice = DynamicCast<TdmaNetDevice>(dev);

  float* reward = m_tdmaDevice->GetTdmaController()->GetRLReward(m_slotNum);
 
  for (uint32_t i=0;i<3;i++)
  {
     if (*(reward+i) <= -100)
     {
       m_repeatChoose++;   
     }
  }
  
  
  
  if (m_repeatChoose >= 3)
  {
     isGameOver = true;
  }
*/    
  return isGameOver;
}


/*
Collect observations
*/
Ptr<OpenGymDataContainer>
TdmaGymEnv::GetObservation()
{
  NS_LOG_FUNCTION (this);

  NS_LOG_UNCOND("Now: "<<Simulator::Now().GetNanoSeconds ());
  Ptr<Node> node = NodeList::GetNode (m_slotNum);
  Ptr<NetDevice> dev = node-> GetDevice(0);
  Ptr<TdmaNetDevice> m_tdmaDevice = DynamicCast<TdmaNetDevice>(dev);
  // Get slot usage table
  std::vector<std::pair<uint32_t,uint32_t> > nodeUsedList = m_tdmaDevice->GetTdmaController()->GetNodeUsedList(m_slotNum);
  // Get routing table
  std::vector<ns3::olsr::RoutingTableEntry> tdmaRoutingTable = node->GetObject<ns3::olsr::RoutingProtocol> ()->GetRoutingTableEntries() ;
  // Get queued information
  std::vector<std::pair<Ipv4Address, uint32_t>> queuePktStatus = m_tdmaDevice->GetTdmaController()->GetQueuePktStatus(m_slotNum);
  std::vector<std::pair<Ipv4Address, uint32_t>> twoHopsPktStatus;
  // Get total queued bytes
  uint32_t queuingBytes = m_tdmaDevice->GetTdmaController()->GetQueuingBytes(m_slotNum);
  
  
  // Calculate weight vector
  // Filter one-hop packets
  for (uint32_t i=0;i<queuePktStatus.size();i++)
  {
	for(uint32_t j=0;j<tdmaRoutingTable.size();j++)
	{
		if (tdmaRoutingTable[j].destAddr == queuePktStatus[i].first && tdmaRoutingTable[j].distance >= 2)
		{
            twoHopsPktStatus.push_back(std::pair<Ipv4Address,uint32_t> (tdmaRoutingTable[j].nextAddr,queuePktStatus[i].second));
			break;
		}
	}
  }

  int32_t nodeUsedList_top3Pkt[32];
  std::fill_n(nodeUsedList_top3Pkt,32,4);

  for (uint32_t i=0;i<32;i++)
  {
	if (nodeUsedList[i].first == 0)
	{
		nodeUsedList_top3Pkt[i] = 0;
	}
  }

  uint32_t counter = 1;
  bool isInList = false;
  std::vector<uint32_t> top3PktSize;
                                       
  for (uint32_t i=0;i<twoHopsPktStatus.size();i++)
  {
    // change ip->nodeId
    uint32_t topN_nodeId = twoHopsPktStatus[i].first.CombineMask(Ipv4Mask(255)).Get()-1;
    isInList = false;

	for (uint32_t j=0;j<nodeUsedList.size();j++)
	{
        // replace NodeId with priority
		if(topN_nodeId == nodeUsedList[j].second && nodeUsedList[j].first != 0) // node is top 3 
		{
            isInList = true;
			nodeUsedList_top3Pkt[j] = counter;
            
		}
	}
    if (isInList){
        counter++;
        top3PktSize.push_back(twoHopsPktStatus[i].second);
    }
      
    if (counter >= 4){
        break;
    }


  }

  
  // Store weight vector in box, which is used to send message to python
  uint32_t dataSlotNum = 32;
  std::vector<uint32_t> shape = {dataSlotNum,};
  Ptr<OpenGymBoxContainer<int32_t> > slotUsedTable_box = CreateObject<OpenGymBoxContainer<int32_t> >(shape);

  for (uint32_t i=0;i<32;i++) //nodeUsedList.size()
  {
	slotUsedTable_box->AddValue (nodeUsedList_top3Pkt[i]);
  }

  
  std::vector<uint32_t> shape2 = {3+1,};
  Ptr<OpenGymBoxContainer<uint32_t>> pktBytes_box = CreateObject<OpenGymBoxContainer<uint32_t> >(shape2);

  // Store total packet bytes
  pktBytes_box->AddValue (queuingBytes);
    
  // Store Top K packetbytes in box
  for (uint32_t i=0;i<top3PktSize.size();i++)
  {
	pktBytes_box->AddValue (top3PktSize[i]);
  }

  Ptr<OpenGymTupleContainer> data = CreateObject<OpenGymTupleContainer> ();
  data->Add(slotUsedTable_box);
  data->Add(pktBytes_box);

  Ptr<OpenGymBoxContainer<int32_t> > mslotUsedTablebox = DynamicCast<OpenGymBoxContainer<int32_t> >(data->Get(0));
  Ptr<OpenGymBoxContainer<uint32_t> > mpktBytesbox = DynamicCast<OpenGymBoxContainer<uint32_t>>(data->Get(1));
  NS_LOG_UNCOND ("MyGetObservation: " << data);
  NS_LOG_UNCOND ("---" << mslotUsedTablebox);
  NS_LOG_UNCOND ("---" << mpktBytesbox);

  
  return data;
}

/*
Define reward function
*/
float
TdmaGymEnv::GetReward()
{
  NS_LOG_FUNCTION (this);

  float reward = 0;
  
  return reward;

}

/*
Define extra info. Optional
Send r_i, throughput, delay to python
*/
 
std::string
TdmaGymEnv::GetExtraInfo()
{
  NS_LOG_FUNCTION (this);
  
  uint32_t previous_slotNum = m_slotNum == 0 ? 15 : m_slotNum - 1;
  Ptr<Node> node = NodeList::GetNode (previous_slotNum);
  Ptr<NetDevice> dev = node-> GetDevice(0);
  Ptr<TdmaNetDevice> m_tdmaDevice = DynamicCast<TdmaNetDevice>(dev);

  float* reward = m_tdmaDevice->GetTdmaController()->GetRLReward(previous_slotNum);
  int64_t tdmaDataBytes = 0;
  
  if (Simulator::Now().GetSeconds () < 6) tdmaDataBytes = 0;
  else tdmaDataBytes = recvBytes / 16 / (Simulator::Now().GetSeconds () - 10); // Initializing phase will not compute the performance
  //NS_LOG_UNCOND("Now: "<<Simulator::Now().GetNanoSeconds ());
  
  uint64_t avgDelay = 0;
  if (recvPacket == 0) avgDelay = 0;
  else avgDelay = totalDelay/recvPacket;
    
    
  std::stringstream stream;
  stream << std::fixed << std::setprecision(2) << *(reward) << ",";
  stream << std::fixed << std::setprecision(2) << *(reward+1) << ",";
  stream << std::fixed << std::setprecision(2) << *(reward+2) << ",";
  stream << tdmaDataBytes<<",";
  stream << avgDelay;
  std::string Info = stream.str();
  

  m_tdmaDevice->GetTdmaController()->ResetRLReward(previous_slotNum);
    
  //std::string Info = std::to_string(m_slotNum);
  NS_LOG_UNCOND("MyGetExtraInfo: " << Info);
  return Info;
}


/*
Execute received actions
*/
bool
TdmaGymEnv::ExecuteActions(Ptr<OpenGymDataContainer> action)
{
  NS_LOG_FUNCTION (this);
  NS_LOG_UNCOND ("ExecuteActions: " << action);
  
  Ptr<Node> node = NodeList::GetNode (m_slotNum);
  Ptr<NetDevice> dev = node-> GetDevice(0);
  Ptr<TdmaNetDevice> m_tdmaDevice = DynamicCast<TdmaNetDevice>(dev);  
    
  uint32_t max_slots = 3;

  Ptr<OpenGymBoxContainer<int32_t> > box = DynamicCast<OpenGymBoxContainer<int32_t> >(action);

  for (uint32_t i=0;i<max_slots;i++)
  {
	if (box->GetValue(i) != -1)
	{
		m_tdmaDevice->GetTdmaController()->SetRLAction(box->GetValue(i));
	}

  }  


  return true;
}


} // ns3 namespace


