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
#include "ns3/packet.h"
#include "ns3/log.h"
#include "tdma-mac-low.h"
#include "ns3/wifi-mac-trailer.h"
#include <cstring>
#include <sstream>
#include "ns3/olsr-header.h"
#include "ns3/header.h"

NS_LOG_COMPONENT_DEFINE ("TdmaMacLow");

#undef NS_LOG_APPEND_CONTEXT
#define NS_LOG_APPEND_CONTEXT std::clog << "[TdmaMac=" << m_self << "] "


namespace ns3 {

TdmaMacLow::TdmaMacLow ()
  : m_currentPacket (0)
{
}

TdmaMacLow::~TdmaMacLow ()
{
}


void
TdmaMacLow::DoDispose (void)
{
  NS_LOG_FUNCTION (this);
  m_channel = 0;
  m_device = 0;
}

void
TdmaMacLow::SetAddress (Mac48Address ad)
{
  m_self = ad;
}

void
TdmaMacLow::SetBssid (Mac48Address bssid)
{
  m_bssid = bssid;
}
Mac48Address
TdmaMacLow::GetAddress (void) const
{
  return m_self;
}

Mac48Address
TdmaMacLow::GetBssid (void) const
{
  return m_bssid;
}

void
TdmaMacLow::SetRxCallback (Callback<void,Ptr<Packet>,const WifiMacHeader *> callback)
{
  m_rxCallback = callback;
}

Ptr<SimpleWirelessChannel>
TdmaMacLow::GetChannel (void) const
{
  return m_channel;
}

void
TdmaMacLow::SetChannel (Ptr<SimpleWirelessChannel> channel)
{
  NS_LOG_FUNCTION (channel);
  m_channel = channel;
  m_channel->Add (this);
}

Ptr<TdmaNetDevice>
TdmaMacLow::GetDevice (void) const
{
  return m_device;
}

void
TdmaMacLow::SetDevice (Ptr<TdmaNetDevice> device)
{
  NS_LOG_FUNCTION (device);
  m_device = device;
}

void
TdmaMacLow::StartTransmission (Ptr<const Packet> packet,
                               const WifiMacHeader* hdr)
{
  NS_LOG_FUNCTION (this << packet << hdr);
  m_currentPacket = packet->Copy ();
  m_currentHdr = *hdr;

  NS_LOG_DEBUG ("startTx size=" << GetSize (m_currentPacket, &m_currentHdr) <<
                ", to=" << m_currentHdr.GetAddr1 ());
  m_currentPacket->AddHeader (m_currentHdr);
  WifiMacTrailer fcs;
  m_currentPacket->AddTrailer (fcs);
  ForwardDown (m_currentPacket, &m_currentHdr);
  m_currentPacket = 0;
}

void
TdmaMacLow::Receive (Ptr<Packet> packet)
{
  NS_LOG_DEBUG (*packet);

  uint32_t pktSize = packet->GetSize();

  m_device->GetTdmaController()->DeletePacketType(packet->GetUid());

  WifiMacHeader hdr;
  packet->RemoveHeader (hdr);

 
  if (hdr.IsData () || hdr.IsMgt ())
    {
      NS_LOG_DEBUG ("rx group from=" << hdr.GetAddr2 ());
      WifiMacTrailer fcs;
      packet->RemoveTrailer (fcs);

      int64_t start = m_device->GetTdmaController()->GetPacketStart(packet->GetUid());
      m_rxCallback (packet, &hdr);

      // check received packet is for me or not
      if( (hdr.GetAddr1() != m_self && !hdr.GetAddr1().IsBroadcast())) return;


      // parse packet content
      uint8_t *buffer = new uint8_t[packet->GetSize ()];
      packet->CopyData(buffer, packet->GetSize ());
      std::string s = std::string((char*)buffer);
     
      if(s.compare(0,10,"#TDMAUSED#") == 0)
      {
	  std::string str = s.substr(10);
	  m_device->GetTdmaController()->UpdateList(str,m_device->GetNode()->GetId());
      }
      else
      {
	  if (!hdr.GetAddr1().IsBroadcast()) 
      	  {
  	  	// Calculate Data Successfully bytes
	      	m_device->GetTdmaController()->AddDataSuccessfulBytes(pktSize);
	      	//m_device->GetTdmaController()->DeletePacketType(packet->GetUid());
	      	m_device->GetTdmaController()->DeletePacketStart(packet->GetUid(),Simulator::Now().GetNanoSeconds());
		
		//std::cout<< "start: " << start <<", end: " << Simulator::Now().GetNanoSeconds() << std::endl;
		m_device->GetTdmaController()->AddDelay(Simulator::Now ().GetNanoSeconds () - start);
	      	m_device->GetTdmaController()->AddpktCount();	
	 }
      }
      
    }
  else
    {
      NS_LOG_DEBUG ("Need to drop frame as it is not either data or management frame");
    }
  return;
}

uint32_t
TdmaMacLow::GetSize (Ptr<const Packet> packet, const WifiMacHeader *hdr) const
{
  WifiMacTrailer fcs;
  return packet->GetSize () + hdr->GetSize () + fcs.GetSerializedSize ();
}

void
TdmaMacLow::ForwardDown (Ptr<const Packet> packet, const WifiMacHeader* hdr)
{
  NS_LOG_DEBUG ("send " << hdr->GetTypeString () <<
                ", to=" << hdr->GetAddr1 () <<
                ", size=" << packet->GetSize ());
  
  
//HERE IT IS SIMPLEWIRELESSCHANNEL SEND CALL
  m_channel->Send (packet, this);
}

} // namespace ns3
