/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2010 University of Washington
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
 * Modified by Hemanth Narra <hemanthnarra222@gmail.com> to suit the
 * TDMA implementation.
 */
#include "simple-wireless-channel.h"
#include "ns3/simulator.h"
#include "ns3/packet.h"
#include "ns3/node.h"
#include "ns3/log.h"
#include "ns3/double.h"
#include "ns3/uinteger.h"
#include "ns3/ptr.h"
#include "ns3/net-device.h"
#include "ns3/mobility-model.h"

NS_LOG_COMPONENT_DEFINE ("SimpleWirelessChannel");

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (SimpleWirelessChannel);


//std::vector<std::pair<EventId,Mac48Address>>  SimpleWirelessChannel::m_TransmissionEvent;
std::vector<EventInfo>  SimpleWirelessChannel::m_TransmissionEvent;

TypeId
SimpleWirelessChannel::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::SimpleWirelessChannel")
    .SetParent<Channel> ()
    .AddConstructor<SimpleWirelessChannel> ()
    .AddAttribute ("MaxRange",
                   "Maximum Transmission Range (meters)",
                   DoubleValue (250),
                   MakeDoubleAccessor (&SimpleWirelessChannel::m_range),
                   MakeDoubleChecker<double> ())
  ;
  return tid;
}

SimpleWirelessChannel::SimpleWirelessChannel ()
  : m_range (0)

{

}

void
SimpleWirelessChannel::Send (Ptr<const Packet> p, Ptr<TdmaMacLow> sender)
{
  NS_LOG_FUNCTION (p << sender);

  bool isCollision;
  std::vector<EventInfo> tmp_TransmissionEvent; 

  for (TdmaMacLowList::const_iterator i = m_tdmaMacLowList.begin (); i != m_tdmaMacLowList.end (); ++i)
    {
      Ptr<TdmaMacLow> tmp = *i;
      if (tmp->GetDevice () == sender->GetDevice ())
        {
          continue;
        }
      Ptr<MobilityModel> a = sender->GetDevice ()->GetNode ()->GetObject<MobilityModel> ();
      Ptr<MobilityModel> b = tmp->GetDevice ()->GetNode ()->GetObject<MobilityModel> ();
      NS_ASSERT_MSG (a && b, "Error:  nodes must have mobility models");
      double distance = a->GetDistanceFrom (b);
      NS_LOG_DEBUG ("Distance: " << distance << " Max Range: " << m_range);
    
	if (distance > m_range)
        {
          continue;
        }
      
      WifiMacHeader hdr;
      p->PeekHeader(hdr);


      // speed of light is 3.3 ns/meter
      Time propagationTime = NanoSeconds (uint64_t (3.3 * distance));
      
/*      
      NS_LOG_UNCOND ("Time: "<<Simulator::Now ().GetNanoSeconds () << "ns, Node " << sender->GetDevice ()->GetNode ()->GetId () << " sending to node " <<
                    tmp->GetDevice ()->GetNode ()->GetId () << " at distance " << distance <<
                    " meters; arriving time (ns): " << propagationTime << ", packet Size: " << p->GetSize () << ", Receiver: " << hdr.GetAddr1());

      std::cout <<  "Time: "<<Simulator::Now ().GetNanoSeconds () << "ns, Node " << sender->GetDevice ()->GetNode ()->GetId () << " sending to node " <<
                    tmp->GetDevice ()->GetNode ()->GetId () << " at distance " << distance <<
                    " meters; arriving time (ns): " << propagationTime << ", packet Size: " << p->GetSize () << ", Receiver: " << hdr.GetAddr1() <<
		    ", pktUid: " << p->GetUid() << std::endl;
*/ 
          
      
      NS_LOG_DEBUG ("Node " << sender->GetDevice ()->GetNode ()->GetId () << " sending to node " <<
                    tmp->GetDevice ()->GetNode ()->GetId () << " at distance " << distance <<
                    " meters; arriving time (ns): " << propagationTime);


      isCollision = false; 
     

      
      // check the sending now collide with the scheduled sending event
      for (uint32_t i = 0; i < m_TransmissionEvent.size();i++)
	{
	   // remove expired( remain_time = 0 ) event
	   if ( Simulator::GetDelayLeft(m_TransmissionEvent[i].eventId) == Seconds(0.0) )
	   {
		m_TransmissionEvent.erase(m_TransmissionEvent.begin () + i);
		i--;
		continue;
	   }

	   // check collision
	   if ( m_TransmissionEvent[i].receiver == tmp->GetDevice()->GetMac()->GetAddress() && m_TransmissionEvent[i].sender != sender->GetDevice()->GetMac()->GetAddress())
           {
		NS_LOG_UNCOND ("Collision occurred, From node: " << sender->GetDevice()->GetNode()->GetId() << " to Node: " << tmp->GetDevice()->GetNode()->GetId() );
		std::cout << "Collision occurred, From node: " << sender->GetDevice()->GetNode()->GetId() << " to Node: " << tmp->GetDevice()->GetNode()->GetId() << std::endl;
		Simulator::Cancel(m_TransmissionEvent[i].eventId);
		isCollision = true;
	   }
	  
	}

      
      //Simulator::ScheduleWithContext (tmp->GetDevice ()->GetNode ()->GetId (),(propagationTime),
      //                                &TdmaMacLow::Receive, tmp, p->Copy ());
      

      // if not collision, add this event into tmp vector
      EventInfo info;
      info.eventId = Simulator::Schedule ((propagationTime), &SimpleWirelessChannel::CopyPacket,this,tmp, p->Copy ());
      info.sender = sender->GetDevice()->GetMac()->GetAddress();
      info.receiver = tmp->GetDevice()->GetMac()->GetAddress();
      
      tmp_TransmissionEvent.push_back (info);
      

      // Still add the Cancelled event into queue to prevent the later events collide with this Event
      if(isCollision)
      {  
		Simulator::Cancel(tmp_TransmissionEvent[tmp_TransmissionEvent.size()-1].eventId);
		
      }
    }


    // push sending events (this round) into m_TransmissionEvent     
    m_TransmissionEvent.insert (m_TransmissionEvent.end(), tmp_TransmissionEvent.begin(), tmp_TransmissionEvent.end() );
    tmp_TransmissionEvent.clear();
}

void
SimpleWirelessChannel::CopyPacket (Ptr<TdmaMacLow> tmp, Ptr<const Packet> p)
{
      Simulator::ScheduleWithContext (tmp->GetDevice ()->GetNode ()->GetId (),Seconds(0.0),
                                      &TdmaMacLow::Receive, tmp, p->Copy ());
}

void
SimpleWirelessChannel::Add (Ptr<TdmaMacLow> tdmaMacLow)
{
  NS_LOG_DEBUG (this << " " << tdmaMacLow);
  m_tdmaMacLowList.push_back (tdmaMacLow);
  NS_LOG_DEBUG ("current m_tdmaMacLowList size: " << m_tdmaMacLowList.size ());
}


std::size_t
SimpleWirelessChannel::GetNDevices (void) const
{
  return m_tdmaMacLowList.size ();
}
Ptr<NetDevice>
SimpleWirelessChannel::GetDevice (std::size_t i) const
{
  return m_tdmaMacLowList[i]->GetDevice ();
}

double
SimpleWirelessChannel::GetMaxRange (void) const
{
  return m_range;
}

} // namespace ns3
