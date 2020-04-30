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
#include "ns3/assert.h"
#include "ns3/enum.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "tdma-central-mac.h"
#include "tdma-controller.h"
#include "tdma-mac.h"
#include "tdma-mac-low.h"
#include "ns3/abort.h"
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <cstdio>
#include <iomanip>

NS_LOG_COMPONENT_DEFINE ("TdmaController");

#define MY_DEBUG(x) \
  NS_LOG_DEBUG (Simulator::Now () << " " << this << " " << x)

namespace ns3 {
NS_OBJECT_ENSURE_REGISTERED (TdmaController);

Time
TdmaController::GetDefaultSlotTime (void)
{
  return MicroSeconds (1000);
}

Time
TdmaController::GetDefaultCtrlSlotTime (void)
{
  return MicroSeconds (500);
}

Time
TdmaController::GetDefaultGuardTime (void)
{
  return MicroSeconds (0);
}

DataRate
TdmaController::GetDefaultDataRate (void)
{
  NS_LOG_DEBUG ("Setting default");
  return DataRate ("54000000b/s");
}

/*************************************************************
 * Tdma Controller Class Functions
 ************************************************************/
TypeId
TdmaController::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3:TdmaController")
    .SetParent<Object> ()
    .AddConstructor<TdmaController> ()
    .AddAttribute ("DataRate",
                   "The default data rate for point to point links",
                   DataRateValue (GetDefaultDataRate ()),
                   MakeDataRateAccessor (&TdmaController::SetDataRate,
                                         &TdmaController::GetDataRate),
                   MakeDataRateChecker ())
    .AddAttribute ("SlotTime", "The duration of a Slot in microseconds.",
                   TimeValue (GetDefaultSlotTime ()),
                   MakeTimeAccessor (&TdmaController::SetSlotTime,
                                     &TdmaController::GetSlotTime),
                   MakeTimeChecker ())
    .AddAttribute ("CtrlSlotTime", "The duration of a Control Slot in microseconds.",
                   TimeValue (GetDefaultCtrlSlotTime ()),
                   MakeTimeAccessor (&TdmaController::SetCtrlSlotTime,
                                     &TdmaController::GetCtrlSlotTime),
                   MakeTimeChecker ())
    .AddAttribute ("GuardTime", "GuardTime between TDMA slots in microseconds.",
                   TimeValue (GetDefaultGuardTime ()),
                   MakeTimeAccessor (&TdmaController::SetGuardTime,
                                     &TdmaController::GetGuardTime),
                   MakeTimeChecker ())
    .AddAttribute ("InterFrameTime", "The wait time between consecutive tdma frames.",
                   TimeValue (MicroSeconds (0)),
                   MakeTimeAccessor (&TdmaController::SetInterFrameTimeInterval,
                                     &TdmaController::GetInterFrameTimeInterval),
                   MakeTimeChecker ())
    .AddAttribute ("TdmaMode","Tdma Mode, Centralized",
                   EnumValue (CENTRALIZED),
                   MakeEnumAccessor (&TdmaController::m_tdmaMode),
                   MakeEnumChecker (CENTRALIZED, "Centralized"));
  return tid;
}

TdmaController::TdmaController ()
  : m_slotTime (0),
    m_ctrlslotTime (0),
		m_guardTime (0),
		m_tdmaFrameLength (0),
		m_tdmaInterFrameTime (0),
		m_totalSlotsAllowed (10000),
		m_activeEpoch (false),
		m_tdmaMode (CENTRALIZED),
    m_channel (0),
    m_usedslotPenalty (-10),
    m_collisionPenalty (-0.02),
    m_delayReward (10),
    m_queuingBytesThreshold (GetSlotTime()*GetDataRate() / 8 * (1/3))
{
  NS_LOG_FUNCTION (this);
  //LogComponentEnable ("TdmaController", LOG_LEVEL_DEBUG);
  NS_LOG_DEBUG("TdmaController constructor");

  m_tdmaDataBytes = 0;

  for (uint32_t i=0;i<16;i++)
  {
      m_tdmaCtrlSlotMap[i] = i;
      m_tdmaEntrySlotNum[i] = -1;
  }

  m_tdmaCtrlSlotMapRev[0] = 0;
  for (uint32_t i=1;i<16;i++)
  {
      m_tdmaCtrlSlotMapRev[m_tdmaCtrlSlotMap[i]] = i;
  }

  for (uint32_t i=0;i<16;i++)
  {
	for (uint32_t j=0;j<32;j++)
	{
     		m_tdmaUsedListPre[i][j] = std::make_pair(0,0);
		m_tdmaUsedListCur[i][j] = std::make_pair(0,0);
	}
  }

  memset(m_rlReward,0,16*3*sizeof(int32_t));
  

  // random backoff 
  //srand( 30000 );
}

TdmaController::~TdmaController ()
{
  m_channel = 0;
  m_bps = 0;
  m_slotPtrs.clear ();
}

void
TdmaController::Start (void)
{
  NS_LOG_FUNCTION (this);
  if (!m_activeEpoch)
    {
      m_activeEpoch = true;
      Simulator::Schedule (NanoSeconds (10),&TdmaController::StartTdmaSessions, this);
    }
}

void
TdmaController::StartTdmaSessions (void)
{
  NS_LOG_FUNCTION_NOARGS ();

  ClearTdmaDataSlot();
  
  //printf("Time: %luns, Overhead: %f, Delay: %fns, Throughput: %f B/s, Delivery Ratio: %f\n",Simulator::Now().GetNanoSeconds(),(double)m_tdmaNonDataBytes/m_tdmaTotalBytes,(double)m_tdmaDelay/m_pktCount, (double)(m_tdmaTotalBytes-m_tdmaNonDataBytes)/(Simulator::Now().GetSeconds()),(double)m_tdmaDataSuccessfulBytes/m_tdmaDataBytes);
  std::cout<<"--------------------------Frame Start : "<< Simulator::Now().GetNanoSeconds() <<"ns ---------------------------"<<std::endl;
  NS_LOG_UNCOND("--------------------------Frame Start : "<< Simulator::Now().GetNanoSeconds() <<"ns ---------------------------");
  ScheduleTdmaSession (0);
  
  //ShiftCtrlSlot();
}

// Rotate the current frame's UsedList to previous frame's UsedList
void 
TdmaController::RotateUsedList (void)
{
  for (uint32_t i=0;i<16;i++)
  {
	for (uint32_t j=0;j<32;j++)
	{
     		m_tdmaUsedListPre[i][j] = m_tdmaUsedListCur[i][j]; // Rotate
		m_tdmaUsedListCur[i][j] = std::make_pair(0,0);
	}
  }
}

void
TdmaController::AddTdmaSlot (uint32_t slotPos, Ptr<TdmaMac> macPtr, uint32_t nodeId)
{
  NS_LOG_FUNCTION (slotPos << macPtr);
  std::map<uint32_t, std::vector<Ptr<TdmaMac> > >::iterator it = m_slotPtrs.find (slotPos);
  if (it == m_slotPtrs.end ()) // this slot is not used, insert a new pair of slotPos and mac vector
    {
   	std::pair<std::map<uint32_t, std::vector< Ptr<TdmaMac> > >::iterator, bool> result =
    		m_slotPtrs.insert (std::make_pair (slotPos,std::vector<Ptr<TdmaMac> >{macPtr})); 

	m_mac2Id.insert(std::make_pair(macPtr,nodeId));
	m_id2mac.insert(std::make_pair(nodeId,macPtr));

	if (result.second == true)
    	{
		NS_LOG_DEBUG ("Slot " << slotPos << " is not used, Added mac :" << macPtr);
    	}
  	else
    	{
      		NS_LOG_WARN ("Could not add mac: " << macPtr << " to slot " << slotPos);
    	}


    }
  else // push current node mac into vector
    {
	it->second.push_back(macPtr);
	NS_LOG_DEBUG ("Slot " << slotPos << " is used, Added mac :" << macPtr);
	
	m_mac2Id.insert(std::make_pair(macPtr,nodeId));
	m_id2mac.insert(std::make_pair(nodeId,macPtr));
		
    }

}


void
TdmaController::DeleteTdmaSlot (uint32_t slotPos, Ptr<TdmaMac> macPtr)
{
  std::map<uint32_t, std::vector<Ptr<TdmaMac> > >::iterator it = m_slotPtrs.find (slotPos);
  if (it != m_slotPtrs.end ())
    {
	// Check macPtr is in the vector or not
	std::vector<Ptr<TdmaMac> >::iterator it_vec = find(it->second.begin(),it->second.end(),macPtr);
	if (it_vec != it->second.end())
	{
		it->second.erase(it_vec);
		//printf("delete successfully\n");
	}
    }
}

void
TdmaController::ClearTdmaDataSlot ()
{
  for (uint32_t i=16;i<48;i++)
  {
  	std::map<uint32_t, std::vector<Ptr<TdmaMac> > >::iterator it = m_slotPtrs.find (i);
  	if (it != m_slotPtrs.end ())
    	{
		it->second.clear();
    	}
  }
}

void
TdmaController::SetSlotTime (Time slotTime)
{
  NS_LOG_FUNCTION (this << slotTime);
  m_slotTime = slotTime.GetMicroSeconds ();
}

Time
TdmaController::GetSlotTime (void) const
{
  return MicroSeconds (m_slotTime);
}

void
TdmaController::SetCtrlSlotTime (Time slotTime)
{
  NS_LOG_FUNCTION (this << slotTime);
  m_ctrlslotTime = slotTime.GetMicroSeconds ();
}

Time
TdmaController::GetCtrlSlotTime (void) const
{
  return MicroSeconds (m_ctrlslotTime);
}


void
TdmaController::SetDataRate (DataRate bps)
{
  NS_LOG_FUNCTION (this << bps);
  m_bps = bps;
}

DataRate
TdmaController::GetDataRate (void) const
{
  return m_bps;
}

void
TdmaController::SetChannel (Ptr<SimpleWirelessChannel> c)
{
  NS_LOG_FUNCTION (this << c);
  m_channel = c;
}


Ptr<SimpleWirelessChannel>
TdmaController::GetChannel (void) const
{
  NS_LOG_FUNCTION (this);
  return m_channel;
}

void
TdmaController::SetGuardTime (Time guardTime)
{
  NS_LOG_FUNCTION (this << guardTime);
  //guardTime is based on the SimpleWirelessChannel's max range
  if (m_channel != 0)
    {
      m_guardTime = Seconds (m_channel->GetMaxRange () / 300000000.0).GetMicroSeconds ();
    }
  else
    {
      m_guardTime = guardTime.GetMicroSeconds ();
    }
}

Time
TdmaController::GetGuardTime (void) const
{
  return MicroSeconds (m_guardTime);
}

void
TdmaController::SetInterFrameTimeInterval (Time interFrameTime)
{
  NS_LOG_FUNCTION (interFrameTime);
  m_tdmaInterFrameTime = interFrameTime.GetMicroSeconds ();
}

Time
TdmaController::GetInterFrameTimeInterval (void) const
{
  return MicroSeconds (m_tdmaInterFrameTime);
}

void
TdmaController::SetTotalSlotsAllowed (uint32_t slotsAllowed)
{
  m_totalSlotsAllowed = slotsAllowed;
  m_slotPtrs.clear ();
}

uint32_t
TdmaController::GetTotalSlotsAllowed (void) const
{
  return m_totalSlotsAllowed;
}

void
TdmaController::ScheduleTdmaSession (const uint32_t slotNum)
{
  NS_LOG_FUNCTION (slotNum);
  std::map<uint32_t, std::vector<Ptr<TdmaMac> > >::iterator it = m_slotPtrs.find (slotNum);
  
  //NS_LOG_UNCOND("slotNum: "<<slotNum);
  
  bool isCtrl =  (slotNum < m_nNodes );
  Time slotTime;

  // Get slotTime
  if (slotNum < m_nNodes)
  {
	slotTime = GetCtrlSlotTime();
  }
  else
  {
	slotTime = GetSlotTime();
  }

  if (slotNum == m_nNodes) RotateUsedList();
  
  if (it == m_slotPtrs.end ()) // check the current slot is used by any nodes or not
    {
	NS_LOG_WARN ("No MAC ptrs in TDMA controller");
	
    }
  else
   {
	// for each node in the vector, call StartTransmission to start its transmission in this slot time 
  	for (uint32_t k = 0; k < it->second.size() ; k++ ) 
    	{
		// Use current mac to get node id 
		std::map<Ptr<TdmaMac>,uint32_t>::iterator it_id = m_mac2Id.find(it->second[k]);
		if (it_id != m_mac2Id.end() )
		{
			// Use node id to get node device
			std::map<uint32_t,Ptr<TdmaNetDevice> >::iterator it_status = m_tdmaDeviceList.find(it_id->second);
			// If node status is Entry, would call SendUsed to broadcast which slot it want to use.
			if (it_status != m_tdmaDeviceList.end())
			{
				if ( slotNum < m_nNodes  ) // Control slot
				{
					//SendUsed(it_status->second);
				}
			}
		}
		
  		NS_LOG_DEBUG ("mac: " << it->second[k] << " could sending packet in slot " <<slotNum);
  		Time transmissionSlot = MicroSeconds (slotTime.GetMicroSeconds () * 1);
  		NS_ASSERT (it->second[k] != NULL);
		it->second[k]->StartTransmission (transmissionSlot.GetMicroSeconds (), isCtrl);

    	}
   }

  Time totalTransmissionTimeUs = GetGuardTime () + MicroSeconds (slotTime.GetMicroSeconds () * 1);

  if ((slotNum + 1) == GetTotalSlotsAllowed ()) // restart a new frame
    {
      NS_LOG_DEBUG ("Starting over all sessions again");
      Simulator::Schedule ((totalTransmissionTimeUs + GetInterFrameTimeInterval ()), &TdmaController::StartTdmaSessions, this);
    }
  else  // do ScheduleTdmaSession at next slot
    {
      NS_LOG_DEBUG ("Scheduling next session");
      Simulator::Schedule (totalTransmissionTimeUs, &TdmaController::ScheduleTdmaSession, this, (slotNum + 1));
    }
}

Time
TdmaController::CalculateTxTime (Ptr<const Packet> packet)
{
  NS_LOG_FUNCTION (*packet);
  NS_ASSERT_MSG (packet->GetSize () < 1500,"PacketSize must be less than 1500B, it is: " << packet->GetSize ());
  return m_bps.CalculateBytesTxTime (packet->GetSize ());
}

void
TdmaController::AddNetDevice (uint32_t nodeId,Ptr<TdmaNetDevice> device)
{
  m_tdmaDeviceList.insert(std::make_pair(nodeId,device));
}


// use to check current slot type (Control/Data/Entry)
void
TdmaController::SetNodeNum (uint32_t NodeNum)
{
  m_nNodes = NodeNum;
}


// the TDMA used msg type : (priority,nodeId) = (1+2) bytes * 32 slot, the nodeId would be a fixed length (2 bytes),
// e.g. 103 = priority is 1 and nodeId is 3
void
TdmaController::SendUsed (Ptr<TdmaNetDevice> device)
{
  std::ostringstream msg;  msg << "#TDMAUSED#";

  
  // Send previous frame's UsedList to avoid the hidden nodes problem
  for (uint32_t i=0;i<32;i++)
  {
    
    if (m_tdmaUsedListPre[device->GetNode()->GetId()][i].second != device->GetNode()->GetId() && m_tdmaUsedListPre[device->GetNode()->GetId()][i].first != 0)
      msg << m_tdmaUsedListPre[device->GetNode()->GetId()][i].first+1 << std::setfill('0') << std::setw(2) << m_tdmaUsedListPre[device->GetNode()->GetId()][i].second;
    else
      msg << m_tdmaUsedListPre[device->GetNode()->GetId()][i].first << std::setfill('0') << std::setw(2) << m_tdmaUsedListPre[device->GetNode()->GetId()][i].second;
    
  }

  
/*   
  // Search unused slot from UsedList
  std::vector<uint32_t> candidateList_unused;
  std::vector<uint32_t> unusedList_select;

 
  uint32_t num = rand() % 4; // Num of slot the node need to use (0~3)
  
  // Select the the unused slot in previous frame
  if (num != 0)
  {
  	
  	for (uint32_t i=0;i<32;i++)
  	{
		if ((m_tdmaUsedListCur[device->GetNode()->GetId()][i].first ) == 0 ) // Slot is unused
		{
      			candidateList_unused.push_back(i);
		}
		
	}
	
	if (candidateList_unused.size() < num)
  	{
		// Get from candidateList_unused 
  		unusedList_select.insert(unusedList_select.end(),candidateList_unused.begin(),candidateList_unused.begin()+candidateList_unused.size());

 	}
  	else
  	{
		std::random_shuffle (candidateList_unused.begin(),candidateList_unused.end());
	
		// Select unused slot from candidateList_unused according to the num
		unusedList_select.insert(unusedList_select.end(),candidateList_unused.begin(),candidateList_unused.begin()+num);
        }
  
  
	
	sort(unusedList_select.begin(),unusedList_select.end());
  }
  
  */
  
  

  sort(m_tdmaRLAction.begin(),m_tdmaRLAction.end());
  uint32_t counter = 0;


  
  // Use the unused slot in unusedList_select
  for (uint32_t i=0;i<32;i++)
  {
	//if ( counter < num && i == unusedList_select[counter] ) 
	if ( counter < m_tdmaRLAction.size() && i == m_tdmaRLAction[counter] ) 
	{
    if (m_tdmaUsedListCur[device->GetNode()->GetId()][i].first != 0)
    {
      m_rlReward[device->GetNode()->GetId()][counter] += m_usedslotPenalty;
    }
    

		// Choose a unused slot
		m_tdmaUsedListCur[device->GetNode()->GetId()][i] = std::make_pair(1,device->GetNode()->GetId());
    
		//printf("Send:%d node use slot %d\n",(m_tdmaUsedListCur[device->GetNode()->GetId()][i].second),i);
		counter++;
    
  }
	if (m_tdmaUsedListCur[device->GetNode()->GetId()][i].first != 0 && m_tdmaUsedListCur[device->GetNode()->GetId()][i].second != device->GetNode()->GetId())
	  msg << m_tdmaUsedListCur[device->GetNode()->GetId()][i].first+1 << std::setfill('0') << std::setw(2) << m_tdmaUsedListCur[device->GetNode()->GetId()][i].second;
	else
	  msg << m_tdmaUsedListCur[device->GetNode()->GetId()][i].first << std::setfill('0') << std::setw(2) << m_tdmaUsedListCur[device->GetNode()->GetId()][i].second;
  }
  msg << '\0';

  m_tdmaRLAction.clear();

  // Broadcast previous/current UsedList
  Ptr<Packet> packet = Create<Packet> ((uint8_t*) msg.str().c_str(), msg.str().length() + 1);	
  device->SendbyMac(packet,Mac48Address::GetBroadcast(),0);

  // Get node mac for Add/Delete controller slot map
  std::map<uint32_t,Ptr<TdmaMac>>::iterator it_mac = m_id2mac.find(device->GetNode()->GetId());

  // Update the tdma slot map
  for (uint32_t i=0;i<32;i++)
  {
	if (m_tdmaUsedListPre[device->GetNode()->GetId()][i].second == device->GetNode()->GetId() && m_tdmaUsedListPre[device->GetNode()->GetId()][i].first != 0)
	{
		AddTdmaSlot(i+16,it_mac->second,device->GetNode()->GetId());
		//printf("Node %d use slot %d\n",device->GetNode()->GetId(),i);
	}
  }

}

// According to the received Used msg,
// Received node would check the received Used msg is equal to local UsedList,
// If any slot is not equal to UsedList, it would update the List
void
TdmaController::UpdateList (std::string s, uint32_t nodeId)
{

  for(uint32_t i=0;i<s.length();i+=3)
  {
	uint32_t hops = std::stoi(s.substr(i,1));
	uint32_t slot_nodeId = std::stoi(s.substr(i+1,2));

  if (hops >= 3) continue; 

  	// Get node mac for Add/Delete controller slot map
  	std::map<uint32_t,Ptr<TdmaMac>>::iterator it_mac = m_id2mac.find(nodeId);
 	

	if ( i/3 < 32 )  // Previous frame : used to update the UsedList again to avoid the hidden node problem
	{
		if ( hops != 0 && m_tdmaUsedListPre[nodeId][i/3].first == 0 )
		{
			m_tdmaUsedListPre[nodeId][i/3] = std::make_pair(hops,slot_nodeId);
			//printf("Node %d,update previous frame:%d use slot %d\n",nodeId,slot_nodeId,i/3);
		}
		else if ( hops != 0 && m_tdmaUsedListPre[nodeId][i/3].second == nodeId && slot_nodeId != nodeId )
		{
			m_tdmaUsedListPre[nodeId][i/3] = std::make_pair(hops,slot_nodeId);
			DeleteTdmaSlot(i/3+16,it_mac->second); 
			//m_rlReward[nodeId] += m_collisionPenalty;
			//printf("Node %d,delete previous frame:%d use slot %d, this slot is for Node %d\n",nodeId,nodeId,i/3,slot_nodeId);
		}
     
    else if ( hops != 0 && m_tdmaUsedListPre[nodeId][i/3].second != nodeId && m_tdmaUsedListPre[nodeId][i/3].first > hops)
    {
      m_tdmaUsedListPre[nodeId][i/3] = std::make_pair(hops,slot_nodeId);
      //printf("Node %d,update previous frame:%d use slot %d (only update slot map)\n",nodeId,slot_nodeId,i/3);
    }
	}
	else if (i/3 >= 32)
	{
	  	if ( hops != 0 && m_tdmaUsedListCur[nodeId][i/3-32].first == 0 )
		{
			m_tdmaUsedListCur[nodeId][i/3-32] = std::make_pair(hops,slot_nodeId);
			//printf("Node %d,update:%d use slot %d\n",nodeId,slot_nodeId,i/3-32);
		}
		else if ( hops != 0 && m_tdmaUsedListCur[nodeId][i/3-32].second == nodeId && slot_nodeId != nodeId )
		{
			m_tdmaUsedListCur[nodeId][i/3-32] = std::make_pair(hops,slot_nodeId);
            DeleteTdmaSlot(i/3-32+16,it_mac->second); 
			//m_rlReward[nodeId] += m_collisionPenalty;
			//printf("Node %d,delete:%d use slot %d, this slot is for Node %d\n",nodeId,nodeId,i/3-32,slot_nodeId);
		}
    
    else if ( hops != 0 && m_tdmaUsedListCur[nodeId][i/3-32].second != nodeId && m_tdmaUsedListCur[nodeId][i/3-32].first > hops)
    {
      m_tdmaUsedListCur[nodeId][i/3-32] = std::make_pair(hops,slot_nodeId);
      //printf("Node %d,update:%d use slot %d (only update slot map)\n",nodeId,slot_nodeId,i/3-32);
    }

	}
 }

}



uint32_t
TdmaController::GetCtrlNode (uint32_t slotNum)
{
  return m_tdmaCtrlSlotMap[slotNum];
}

uint32_t
TdmaController::GetCtrlSlot (uint32_t nodeId)
{
  return m_tdmaCtrlSlotMapRev[nodeId];
}

void
TdmaController::ShiftCtrlSlotMap()
{
  uint32_t small_scope = 3;
  uint32_t large_scope = 7;
  uint32_t whole_scope = 63/(large_scope*small_scope);
	
  // level 1
  for (uint32_t i=1;i<64;i+=small_scope)
  {
      uint32_t temp = m_tdmaCtrlSlotMap[i+small_scope-1];
      memmove(&m_tdmaCtrlSlotMap[i+1], &m_tdmaCtrlSlotMap[i], sizeof(uint32_t)*(small_scope-1));
      m_tdmaCtrlSlotMap[i] = temp;
  }

  // level 2
  for(uint32_t i=1;i<64;i+=large_scope*small_scope)
  {
     uint32_t temp[small_scope];
     memmove(&temp[0],&m_tdmaCtrlSlotMap[i+small_scope*(large_scope-1)],sizeof(uint32_t)*(small_scope));
     memmove(&m_tdmaCtrlSlotMap[i+small_scope], &m_tdmaCtrlSlotMap[i], sizeof(uint32_t)*(small_scope*(large_scope-1)));
     memmove(&m_tdmaCtrlSlotMap[i],&temp[0],sizeof(uint32_t)*(small_scope));
  }

  // level 3
  uint32_t temp[small_scope*large_scope];
  memmove(&temp[0],&m_tdmaCtrlSlotMap[1+small_scope*large_scope*(whole_scope-1)],sizeof(uint32_t)*(small_scope*large_scope));
  memmove(&m_tdmaCtrlSlotMap[1+small_scope*large_scope], &m_tdmaCtrlSlotMap[1], sizeof(uint32_t)*(small_scope*large_scope*(whole_scope-1)));
  memmove(&m_tdmaCtrlSlotMap[1],&temp[0],sizeof(uint32_t)*(small_scope*large_scope));

  for(uint32_t i=1;i<64;i++)
  {
	m_tdmaCtrlSlotMapRev[m_tdmaCtrlSlotMap[i]] = i;
  }
  
}

void
TdmaController::ShiftCtrlSlot (void)
{
  ShiftCtrlSlotMap();

  for (uint32_t i=1;i<64;i++)
  {
      std::map<uint32_t, std::vector<Ptr<TdmaMac> > >::iterator it = m_slotPtrs.find (i);
      if(it != m_slotPtrs.end())
      {
          it->second.clear();

          std::map<uint32_t,Ptr<TdmaMac>>::iterator it_mac = m_id2mac.find(GetCtrlNode(i));
          it->second.push_back(it_mac->second);
      }
      else
      {
          std::map<uint32_t,Ptr<TdmaMac>>::iterator it_mac = m_id2mac.find(GetCtrlNode(i));

   	  m_slotPtrs.insert (std::make_pair (i,std::vector<Ptr<TdmaMac> >{it_mac->second})); 

      }
  } 
}

std::vector<std::pair<Ipv4Address, uint32_t>>
TdmaController::GetTop3QueuePktStatus (uint32_t nodeId)
{
  std::map<uint32_t,Ptr<TdmaMac>>::iterator it_mac = m_id2mac.find(nodeId);

  NS_ASSERT (it_mac != m_id2mac.end());
  return it_mac->second->GetQueuePktStatus ();
}

uint32_t
TdmaController::GetQueuingBytes (uint32_t nodeId)
{
  std::map<uint32_t,Ptr<TdmaMac>>::iterator it_mac = m_id2mac.find(nodeId);

  NS_ASSERT (it_mac != m_id2mac.end()); 
  return it_mac->second->GetQueuingBytes ();
}

void 
TdmaController::AddDataBytes (uint64_t bytes)
{
  m_tdmaDataBytes+=bytes;
}

uint64_t
TdmaController::GetDataBytes (void)
{
  return m_tdmaDataBytes;
}


std::vector<std::pair<uint32_t,uint32_t>>
TdmaController::GetNodeUsedList (uint32_t nodeId)
{
  std::vector<std::pair<uint32_t,uint32_t>> nodeUsedList;
  for(uint32_t i=0;i<32;i++)
  {
    nodeUsedList.push_back(m_tdmaUsedListCur[nodeId][i]);
  }

  return nodeUsedList;
}

void
TdmaController::SetRLAction(uint32_t slotNum)
{
  m_tdmaRLAction.push_back(slotNum);
}

float*
TdmaController::GetRLReward(uint32_t nodeId)
{
  return *(m_rlReward+nodeId);
}

void
TdmaController::ResetRLReward(uint32_t nodeId)
{
  memset(m_rlReward+nodeId,0,3*sizeof(int32_t));
}

} // namespace ns3
