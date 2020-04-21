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
#include "ns3/simulator.h"
#include "ns3/uinteger.h"
#include "ns3/log.h"
#include "tdma-mac-queue.h"

#include "ns3/llc-snap-header.h"
#include "ns3/ipv4-header.h"
#include <algorithm>

using namespace std;
NS_LOG_COMPONENT_DEFINE ("TdmaMacQueue");
namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (TdmaMacQueue);

TdmaMacQueue::Item::Item (Ptr<const Packet> packet,
                          const WifiMacHeader &hdr,
                          Time tstamp)
  : packet (packet),
    hdr (hdr),
    tstamp (tstamp)
{
}

TypeId
TdmaMacQueue::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::TdmaMacQueue")
    .SetParent<Object> ()
    .AddConstructor<TdmaMacQueue> ()
    .AddAttribute ("MaxPacketNumber", "If a packet arrives when there are already this number of packets, it is dropped.",
                   UintegerValue (400),
                   MakeUintegerAccessor (&TdmaMacQueue::m_maxSize),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("MaxDelay", "If a packet stays longer than this delay in the queue, it is dropped.",
                   TimeValue (Seconds (10.0)),
                   MakeTimeAccessor (&TdmaMacQueue::m_maxDelay),
                   MakeTimeChecker ())
  ;
  return tid;
}

TdmaMacQueue::TdmaMacQueue ()
   : m_maxSize (0)
{
  NS_LOG_FUNCTION_NOARGS ();
  m_size[0] = 0;
  m_count[0] = 0;
  m_size[1] = 0;
  m_count[1] = 0;
  m_queuingBytes = 0;
//  LogComponentEnable ("TdmaMacQueue", LOG_LEVEL_DEBUG);
}

TdmaMacQueue::~TdmaMacQueue ()
{
  Flush ();
}

void
TdmaMacQueue::SetMaxSize (uint32_t maxSize)
{
  m_maxSize = maxSize;
}

void
TdmaMacQueue::SetMacPtr (Ptr<TdmaMac> macPtr)
{
  m_macPtr = macPtr;
}

void
TdmaMacQueue::SetMaxDelay (Time delay)
{
  m_maxDelay = delay;
}

void
TdmaMacQueue::SetTdmaMacTxDropCallback (Callback<void,Ptr<const Packet> > callback)
{
  m_txDropCallback = callback;
}

uint32_t
TdmaMacQueue::GetMaxSize (void) const
{
  return m_maxSize;
}

Time
TdmaMacQueue::GetMaxDelay (void) const
{
  return m_maxDelay;
}

bool
TdmaMacQueue::Enqueue (Ptr<const Packet> packet, const WifiMacHeader &hdr)
{
  NS_LOG_DEBUG ("Queue Size: " << GetSize (false) <<" Ctrl Queue Size: " << GetSize (true) << " Max Size: " << GetMaxSize ());
  Cleanup (false);
  Cleanup (true);
  Time now = Simulator::Now ();

  if (packet->GetSize() > 1500) return false;

  // parse packet content to push into diff queue
  uint8_t *buffer = new uint8_t[packet->GetSize ()];
  Ptr<Packet> tmp = packet->Copy();
  LlcSnapHeader h;
  tmp->RemoveHeader(h);
  tmp->CopyData(buffer, tmp->GetSize ());
  std::string s = std::string((char*)buffer);
  delete buffer;

  //std::cout<<"PacketUid: "<< packet->GetUid() <<", PacketHeader:  ";
  //std::cout<<packet->ToString()<<std::endl;
  
  std::size_t idx_olsr = packet->ToString().find("olsr");


  bool isCtrl = false;
  if (s.compare(0,5,"#TDMA") == 0 )
  {
  	if (m_size[1] == m_maxSize) return false;
	isCtrl = true;
  }
  else if (idx_olsr != string::npos)
  {
  	if (m_size[1] == m_maxSize) return false;
	isCtrl = true;
  }
  else
  {
  	if (m_size[0] == m_maxSize) return false;
  	isCtrl = false;
    m_queuingBytes += packet->GetSize ();
  }
  
  

  m_queue[isCtrl].push_back (Item (packet, hdr, now));
  m_size[isCtrl]++;
  NS_LOG_DEBUG ("Inserted packet of size: " << packet->GetSize ()
                                            << " uid: " << packet->GetUid ());
  return true;
}

// check queue packet is out-of-date or not
void
TdmaMacQueue::Cleanup (bool isCtrl)
{
  NS_LOG_FUNCTION_NOARGS ();
  if (m_queue[isCtrl].empty ())
    {
      return;
    }
  Time now = Simulator::Now ();
  uint32_t n = 0;
  for (PacketQueueI i = m_queue[isCtrl].begin (); i != m_queue[isCtrl].end (); )
    {
      if (i->tstamp + m_maxDelay > now)
        {
          i++;
        }
      else
        {
          m_count[isCtrl]++;
          NS_LOG_DEBUG (Simulator::Now ().GetSeconds () << "s Dropping this packet as its exceeded queue time, pid: " << i->packet->GetUid ()
                                                        << " macPtr: " << m_macPtr
                                                        << " queueSize: " << m_queue[isCtrl].size ()
                                                        << " count:" << m_count[isCtrl]);
          m_txDropCallback (i->packet);
          if(!isCtrl) 
            m_queuingBytes -= i->packet->GetSize ();
          
	  i = m_queue[isCtrl].erase (i);
          n++;
        }
    }
  m_size[isCtrl] -= n;
}

// pop packet from queue front
Ptr<const Packet>
TdmaMacQueue::Dequeue (WifiMacHeader *hdr, bool isCtrl)
{
  NS_LOG_FUNCTION_NOARGS ();
  Cleanup (true);
  Cleanup (false);
  if (!m_queue[isCtrl].empty ())
    {
      Item i = m_queue[isCtrl].front ();
      m_queue[isCtrl].pop_front ();
      m_size[isCtrl]--;
      if(!isCtrl) 
        m_queuingBytes -= i.packet->GetSize ();
      *hdr = i.hdr;
      NS_LOG_DEBUG ("Dequeued packet of size: " << i.packet->GetSize ());
      return i.packet;
    }
  return 0;
}

// peek queue front packet (not pop)
Ptr<const Packet>
TdmaMacQueue::Peek (WifiMacHeader *hdr, bool isCtrl)
{
  NS_LOG_FUNCTION_NOARGS ();
  Cleanup (true);
  Cleanup (false);
  if (!m_queue[isCtrl].empty ())
    {
      Item i = m_queue[isCtrl].front ();
      *hdr = i.hdr;
      return i.packet;
    }
  return 0;
}

bool
TdmaMacQueue::IsEmpty (bool isCtrl)
{
  Cleanup (isCtrl);
  return m_queue[isCtrl].empty ();
}

uint32_t
TdmaMacQueue::GetSize (bool isCtrl)
{
  return m_size[isCtrl];
}

void
TdmaMacQueue::Flush (void)
{
  m_queue[0].erase (m_queue[0].begin (), m_queue[0].end ());
  m_queue[1].erase (m_queue[1].begin (), m_queue[1].end ());
  m_size[0] = 0;
  m_size[1] = 0;
  m_queuingBytes = 0;
}

Mac48Address
TdmaMacQueue::GetAddressForPacket (enum WifiMacHeader::AddressType type, PacketQueueI it)
{
  if (type == WifiMacHeader::ADDR1)
    {
      return it->hdr.GetAddr1 ();
    }
  if (type == WifiMacHeader::ADDR2)
    {
      return it->hdr.GetAddr2 ();
    }
  if (type == WifiMacHeader::ADDR3)
    {
      return it->hdr.GetAddr3 ();
    }
  return 0;
}

bool
TdmaMacQueue::Remove (Ptr<const Packet> packet, bool isCtrl)
{
  PacketQueueI it = m_queue[isCtrl].begin ();
  for (; it != m_queue[isCtrl].end (); it++)
    {
      if (it->packet == packet)
        {
          m_queue[isCtrl].erase (it);
          m_size[isCtrl]--;
          if(!isCtrl)
            m_queuingBytes -= it->packet->GetSize ();
          return true;
        }
    }
  return false;
}

std::vector<std::pair<Ipv4Address,uint32_t>>
TdmaMacQueue::GetPktStatus ()
{
  std::vector<std::pair<Ipv4Address,uint32_t>> queuePktStatus;
  //std::vector<std::pair<Ipv4Address,uint32_t>> queuePktStatus{std::pair<Ipv4Address,uint32_t>(Ipv4Address("10.1.1.1"),500)};

  LlcSnapHeader h;
  Ipv4Header iph;

  for (PacketQueueI it = m_queue[0].begin (); it != m_queue[0].end(); it++)
  {
    std::size_t idx_ipv4 = it->packet->ToString().find("Ipv4Header");
    if (idx_ipv4 != string::npos)
    {
	Ptr<Packet> copy = it->packet->Copy();
  	copy->RemoveHeader(h);
 	copy->RemoveHeader (iph);
    
    if (queuePktStatus.size() == 0)
    {
        queuePktStatus.push_back(std::pair<Ipv4Address,uint32_t> (iph.GetDestination(),iph.GetPayloadSize()));
    }
    else
    {
        for (uint32_t i=0;i<queuePktStatus.size();i++)
        {
            if (queuePktStatus[i].first == iph.GetDestination()) 
            {
                queuePktStatus[i].second += iph.GetPayloadSize();
                break;
            }
            else if (i == queuePktStatus.size()-1)
            {
                queuePktStatus.push_back(std::pair<Ipv4Address,uint32_t> (iph.GetDestination(),iph.GetPayloadSize()));
            }
        }
    }
    }
  }

  std::sort(queuePktStatus.begin(),queuePktStatus.end(),[](const std::pair<Ipv4Address,uint32_t>& l, const std::pair<Ipv4Address,uint32_t>& r) 
	    {
		//return l.second != r.second ? l.second < r.second : l.first < r.first;
          return l.second >= r.second;
	    });
 
  uint32_t topN = queuePktStatus.size() > 3 ? 3 : queuePktStatus.size(); 
  std::vector<std::pair<Ipv4Address,uint32_t>> top3queuePktStatus(queuePktStatus.begin(),queuePktStatus.begin()+topN);


  return top3queuePktStatus;
  //return queuePktStatus;
}

uint32_t
TdmaMacQueue::GetQueuingBytes (void)
{
  return m_queuingBytes;
}

} // namespace ns3
