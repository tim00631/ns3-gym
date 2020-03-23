#include "tdma-rl-env.h"
#include <sstream>

#include <iostream>
#include <cstring>

namespace ns3 {


NS_LOG_COMPONENT_DEFINE ("TdmaGymEnv");

NS_OBJECT_ENSURE_REGISTERED (TdmaGymEnv);


TdmaGymEnv::TdmaGymEnv ()
{
  NS_LOG_FUNCTION (this);
  m_slotNum = 0;
  m_stepInterval1 = MicroSeconds(500);
  m_stepInterval2 = MicroSeconds(1000*100 + 500);


  for (uint32_t i=0;i<NodeList::GetNNodes();i++)
  {
	Ptr<Node> nodeId = NodeList::GetNode(i);
	Ptr<Ipv4> ipv4 = nodeId->GetObject<Ipv4>();
        Ipv4Address addr = ipv4->GetAddress(1,0).GetLocal();
	m_ip2id.insert(std::pair<Ipv4Address,uint32_t>(addr,i));
  }

  Simulator::Schedule (NanoSeconds(10.0), &TdmaGymEnv::ScheduleNextStateRead, this);

}

TdmaGymEnv::TdmaGymEnv (Time stepInterval1, Time stepInterval2)
{
  NS_LOG_FUNCTION (this);
  m_slotNum = 0;
  m_stepInterval1 = stepInterval1;
  m_stepInterval2 = stepInterval2;


  for (uint32_t i=0;i<NodeList::GetNNodes();i++)
  {
	Ptr<Node> nodeId = NodeList::GetNode(i);
	Ptr<Ipv4> ipv4 = nodeId->GetObject<Ipv4>();
        Ipv4Address addr = ipv4->GetAddress(1,0).GetLocal();
	m_ip2id.insert(std::pair<Ipv4Address,uint32_t>(addr,i));
  }

  Simulator::Schedule (NanoSeconds(10.0), &TdmaGymEnv::ScheduleNextStateRead, this);

}

void
TdmaGymEnv::ScheduleNextStateRead ()
{
  NS_LOG_FUNCTION (this);
  if (m_slotNum <63){
  	Simulator::Schedule (m_stepInterval1, &TdmaGymEnv::ScheduleNextStateRead, this);
  }
  else {
  	Simulator::Schedule (m_stepInterval2, &TdmaGymEnv::ScheduleNextStateRead, this);
  }
  
  Notify();
  
  m_slotNum = m_slotNum >= 63 ? 0 : m_slotNum+1;
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
  uint32_t max_slots = 3; // the max slots the node could choose

  float low = -1;
  float high = 99;
  std::vector<uint32_t> shape = {max_slots,};
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
  // Slot Used Table (size : 100 slots)
  // tdma queue top 3 packet bytes 
  uint32_t dataSlotNum = 100;

  float low = -1;
  float high = 3;
  std::vector<uint32_t> shape = {dataSlotNum, };
  std::string dtype = TypeNameGet<int32_t> ();

  Ptr<OpenGymBoxSpace> slotUsedTable_box = CreateObject<OpenGymBoxSpace> (low, high, shape, dtype);

  uint32_t topNkinds = 3;

  float low2 = 0;
  float high2 = 2000;
  std::vector<uint32_t> shape2 = {topNkinds, };
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
  // Collision more than 3 times
  
  bool isGameOver = false;
  if (Simulator::Now ().GetSeconds () > Seconds(600))
  {
      isGameOver = true;
  } 
  
  return isGameOver;
}


/*
Collect observations
*/
Ptr<OpenGymDataContainer>
TdmaGymEnv::GetObservation()
{
  NS_LOG_FUNCTION (this);

  Ptr<Node> node = NodeList::GetNode (m_slotNum);
  Ptr<NetDevice> dev = node-> GetDevice(0);
  m_tdmaDevice = DynamicCast<TdmaNetDevice>(dev);

  std::vector<std::pair<uint32_t,uint32_t> > nodeUsedList = m_tdmaDevice->GetTdmaController()->GetNodeUsedList(m_slotNum);

  std::vector<ns3::olsr::RoutingTableEntry> tdmaRoutingTable = node->GetObject<ns3::olsr::RoutingProtocol> ()->GetRoutingTableEntries() ;

  std::vector<std::pair<Ipv4Address, uint32_t>> top3queuePktStatus = m_tdmaDevice->GetTdmaController()->GetTop3QueuePktStatus(m_slotNum);

  for (uint32_t i=0;i<top3queuePktStatus.size();i++)
  {
	for(uint32_t j=0;j<tdmaRoutingTable.size();j++)
	{
		if (tdmaRoutingTable[j].destAddr == top3queuePktStatus[i].first)
		{
			top3queuePktStatus[i].first = tdmaRoutingTable[j].nextAddr;
			break;
		}
	}
  }

  int32_t nodeUsedList_top3Pkt[nodeUsedList.size()];
  memset(nodeUsedList_top3Pkt,-1,nodeUsedList.size()*sizeof(int32_t));

  for (uint32_t i=0;i<nodeUsedList.size();i++)
  {
	if (nodeUsedList[i].first == 0)
	{
		nodeUsedList_top3Pkt[i] = 0;
	}
  }

  for (uint32_t i=0;i<top3queuePktStatus.size();i++)
  {
	std::map<Ipv4Address,uint32_t>::iterator it = m_ip2id.find(top3queuePktStatus[i].first);

	if(it!=m_ip2id.end())
	{
		for (uint32_t j=0;j<nodeUsedList.size();j++)
		{
			if (nodeUsedList[j].first == 0)
			{
				nodeUsedList_top3Pkt[j] = 0;
			}
			else if(it->second == nodeUsedList[j].second && nodeUsedList[j].first != 0) // node is top 3 
			{
				nodeUsedList_top3Pkt[j] = i+1;
			}
		}
	}

  }


  uint32_t dataSlotNum = 100;
  std::vector<uint32_t> shape = {dataSlotNum,};
  Ptr<OpenGymBoxContainer<int32_t> > slotUsedTable_box = CreateObject<OpenGymBoxContainer<int32_t> >(shape);

  for (uint32_t i=0;i<nodeUsedList.size();i++)
  {
	//slotUsedTable_box->AddValue (nodeUsedList_top3Pkt[i]);
	slotUsedTable_box->AddValue (nodeUsedList[i].second);
  }


  std::vector<uint32_t> shape2 = {3,};
  Ptr<OpenGymBoxContainer<uint32_t>> pktBytes_box = CreateObject<OpenGymBoxContainer<uint32_t> >(shape2);

  for (uint32_t i=0;i<top3queuePktStatus.size();i++)
  {
	pktBytes_box->AddValue (top3queuePktStatus[i].second);
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

  Ptr<Node> node = NodeList::GetNode (m_slotNum);
  Ptr<NetDevice> dev = node-> GetDevice(0);
  m_tdmaDevice = DynamicCast<TdmaNetDevice>(dev);

  float reward = m_tdmaDevice->GetTdmaController()->GetRLReward(m_slotNum);
  m_tdmaDevice->GetTdmaController()->ResetRLReward(m_slotNum);
  
  return reward;

}

/*
Define extra info. Optional
*/
std::string
TdmaGymEnv::GetExtraInfo()
{
  NS_LOG_FUNCTION (this);
  std::string Info = std::to_string(m_slotNum);
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

  uint32_t max_slots = 3;

  Ptr<OpenGymBoxContainer<int32_t> > box = DynamicCast<OpenGymBoxContainer<int32_t> >(action);

  for (uint32_t i=0;i<max_slots;i++)
  {
	if (box->GetValue(i) != -1)
	{
		m_tdmaDevice->GetTdmaController()->SetRLAction(box->GetValue(i));
	}

  }  

  m_tdmaDevice->GetTdmaController()->SendUsed(m_tdmaDevice);
  return true;
}


} // ns3 namespace


